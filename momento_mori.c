/*
 * Memento Mori for Flipper Zero.
 *
 * Inspired by Oliver Burkeman's "Four Thousand Weeks": the average human life
 * is roughly 4000 weeks long. Enter your date of birth and the app draws all
 * 4000 weeks as an 80x50 pixel grid - one pixel per week - filling in the weeks
 * you have already lived and blinking the one you are living now.
 */
#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <stdio.h>

#include "mm_date.h"

#define TAG "MomentoMori"

#define MM_CONFIG_PATH    APP_DATA_PATH("dob.conf")
#define MM_CONFIG_HEADER  "Momento Mori"
#define MM_CONFIG_VERSION (1)

#define MM_GRID_COLS (80)
#define MM_GRID_ROWS (MM_LIFE_WEEKS / MM_GRID_COLS) /* 50 */
#define MM_GRID_X    (1)
#define MM_GRID_Y    (13)

#define MM_TICK_MS (500)

typedef enum {
    MmScreenGrid,
    MmScreenStats,
    MmScreenEdit,
} MmScreen;

typedef enum {
    MmFieldYear,
    MmFieldMonth,
    MmFieldDay,
    MmFieldCount,
} MmField;

typedef struct {
    FuriMutex* mutex;
    MmScreen screen;
    MmScreen return_screen; /* where to go after leaving the editor */
    bool has_dob;
    MmDate dob;
    MmDate edit;
    MmField field;
    bool blink;
} MmApp;

static const char* const mm_quotes[] = {
    "Memento mori.",
    "Tempus fugit.",
    "Carpe diem.",
    "This week counts.",
    "Life is long enough.",
    "Do it now.",
    "Finitude is a gift.",
    "Spend it well.",
};
#define MM_QUOTE_COUNT (sizeof(mm_quotes) / sizeof(mm_quotes[0]))

/* ---------------------------------------------------------------- helpers */

static void mm_today(MmDate* out) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    out->year = dt.year;
    out->month = dt.month;
    out->day = dt.day;
    if(!mm_date_is_valid(out)) {
        /* RTC not set: fall back to something sane rather than crash the maths. */
        out->year = 2000;
        out->month = 1;
        out->day = 1;
    }
}

static bool mm_load_dob(MmDate* dob) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* header = furi_string_alloc();
    bool ok = false;

    do {
        uint32_t version = 0, year = 0, month = 0, day = 0;
        if(!flipper_format_file_open_existing(ff, MM_CONFIG_PATH)) break;
        if(!flipper_format_read_header(ff, header, &version)) break;
        if(furi_string_cmp_str(header, MM_CONFIG_HEADER) != 0 || version != MM_CONFIG_VERSION)
            break;
        if(!flipper_format_read_uint32(ff, "Year", &year, 1)) break;
        if(!flipper_format_read_uint32(ff, "Month", &month, 1)) break;
        if(!flipper_format_read_uint32(ff, "Day", &day, 1)) break;
        MmDate d = {.year = (uint16_t)year, .month = (uint8_t)month, .day = (uint8_t)day};
        if(!mm_date_is_valid(&d)) break;
        *dob = d;
        ok = true;
    } while(false);

    furi_string_free(header);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static bool mm_save_dob(const MmDate* dob) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    do {
        uint32_t year = dob->year, month = dob->month, day = dob->day;
        if(!flipper_format_file_open_always(ff, MM_CONFIG_PATH)) break;
        if(!flipper_format_write_header_cstr(ff, MM_CONFIG_HEADER, MM_CONFIG_VERSION)) break;
        if(!flipper_format_write_uint32(ff, "Year", &year, 1)) break;
        if(!flipper_format_write_uint32(ff, "Month", &month, 1)) break;
        if(!flipper_format_write_uint32(ff, "Day", &day, 1)) break;
        ok = true;
    } while(false);

    if(!ok) FURI_LOG_E(TAG, "Failed to save date of birth");
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static void mm_enter_editor(MmApp* app) {
    app->return_screen = (app->screen == MmScreenEdit) ? MmScreenGrid : app->screen;
    if(app->has_dob) {
        app->edit = app->dob;
    } else {
        app->edit = (MmDate){.year = 1990, .month = 1, .day = 1};
    }
    app->field = MmFieldYear;
    app->screen = MmScreenEdit;
}

/* ---------------------------------------------------------------- drawing */

static void mm_draw_grid(Canvas* canvas, const MmApp* app, const MmLifeStats* s) {
    char buf[16];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 9, "MEMENTO MORI");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 127, 1, AlignRight, AlignTop, "1/2");

    /* Frame around the 4000 weeks. */
    canvas_draw_frame(canvas, MM_GRID_X - 1, MM_GRID_Y - 1, MM_GRID_COLS + 2, MM_GRID_ROWS + 2);

    int32_t lived = s->weeks_lived;
    if(lived > MM_LIFE_WEEKS) lived = MM_LIFE_WEEKS;

    /* Whole rows of lived weeks, then the partial row. */
    int32_t full_rows = lived / MM_GRID_COLS;
    int32_t rem = lived % MM_GRID_COLS;
    if(full_rows > 0) {
        canvas_draw_box(canvas, MM_GRID_X, MM_GRID_Y, MM_GRID_COLS, full_rows);
    }
    if(rem > 0) {
        canvas_draw_box(canvas, MM_GRID_X, MM_GRID_Y + full_rows, rem, 1);
    }

    /* The current week blinks. */
    if(lived < MM_LIFE_WEEKS && app->blink) {
        canvas_draw_dot(
            canvas, MM_GRID_X + (lived % MM_GRID_COLS), MM_GRID_Y + (lived / MM_GRID_COLS));
    }

    /* Side panel. */
    const int x = 85;
    canvas_set_font(canvas, FontSecondary);
    if(s->weeks_left >= 0) {
        canvas_draw_str(canvas, x, 20, "Left");
        snprintf(buf, sizeof(buf), "%ld", (long)s->weeks_left);
    } else {
        canvas_draw_str(canvas, x, 20, "Bonus");
        snprintf(buf, sizeof(buf), "+%ld", (long)-s->weeks_left);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, x, 31, buf);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, x, 42, "Lived");
    snprintf(buf, sizeof(buf), "%ld", (long)s->weeks_lived);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, x, 53, buf);

    canvas_set_font(canvas, FontSecondary);
    snprintf(
        buf, sizeof(buf), "%u.%u%%", s->permille_lived / 10, s->permille_lived % 10);
    canvas_draw_str(canvas, x, 63, buf);
}

static void mm_draw_stats(Canvas* canvas, const MmLifeStats* s) {
    char buf[32];
    bool bonus = s->weeks_left < 0;

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 1, AlignCenter, AlignTop, bonus ? "BONUS WEEKS" : "WEEKS LEFT OF 4000");
    canvas_draw_str_aligned(canvas, 127, 1, AlignRight, AlignTop, "2/2");

    snprintf(buf, sizeof(buf), "%ld", (long)(bonus ? -s->weeks_left : s->weeks_left));
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignTop, buf);

    /* Life progress bar. */
    const int bar_x = 4, bar_y = 29, bar_w = 120, bar_h = 6;
    canvas_draw_frame(canvas, bar_x, bar_y, bar_w, bar_h);
    int fill = ((bar_w - 2) * s->permille_lived) / 1000;
    if(fill > 0) canvas_draw_box(canvas, bar_x + 1, bar_y + 1, fill, bar_h - 2);

    canvas_set_font(canvas, FontSecondary);
    snprintf(
        buf,
        sizeof(buf),
        "Age %uy %uw  %u.%u%%",
        s->age_years,
        s->age_weeks,
        s->permille_lived / 10,
        s->permille_lived % 10);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop, buf);

    if(bonus) {
        snprintf(buf, sizeof(buf), "%ld days lived", (long)s->days_lived);
    } else {
        snprintf(buf, sizeof(buf), "%ld days left", (long)s->days_left);
    }
    canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignTop, buf);

    /* A different reminder each day. */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas,
        64,
        56,
        AlignCenter,
        AlignTop,
        mm_quotes[(uint32_t)s->days_lived % MM_QUOTE_COUNT]);
}

static void mm_draw_editor(Canvas* canvas, const MmApp* app) {
    char year[8], month[4], day[4];
    snprintf(year, sizeof(year), "%04u", app->edit.year);
    snprintf(month, sizeof(month), "%02u", app->edit.month);
    snprintf(day, sizeof(day), "%02u", app->edit.day);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Date of birth");

    const char* parts[MmFieldCount] = {year, month, day};
    const int y = 32; /* baseline */
    int widths[MmFieldCount];
    int dash_w = canvas_string_width(canvas, "-");
    int gap = 3;
    int total = 0;
    for(int i = 0; i < MmFieldCount; i++) {
        widths[i] = canvas_string_width(canvas, parts[i]);
        total += widths[i];
    }
    total += 2 * (dash_w + 2 * gap);

    int x = (128 - total) / 2;
    for(int i = 0; i < MmFieldCount; i++) {
        if(i == (int)app->field) {
            int cx = x + widths[i] / 2;
            /* Up / down arrows around the selected field. */
            canvas_draw_line(canvas, cx - 3, y - 13, cx, y - 16);
            canvas_draw_line(canvas, cx, y - 16, cx + 3, y - 13);
            canvas_draw_line(canvas, cx - 3, y + 5, cx, y + 8);
            canvas_draw_line(canvas, cx, y + 8, cx + 3, y + 5);

            canvas_draw_box(canvas, x - 2, y - 10, widths[i] + 4, 13);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, x, y, parts[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, x, y, parts[i]);
        }
        x += widths[i];
        if(i < MmFieldCount - 1) {
            x += gap;
            canvas_draw_str(canvas, x, y, "-");
            x += dash_w + gap;
        }
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, "<> field   ^v change");
    canvas_draw_str_aligned(
        canvas, 64, 54, AlignCenter, AlignTop, app->has_dob ? "OK save   Back cancel" : "OK save   Back exit");
}

static void mm_draw_callback(Canvas* canvas, void* ctx) {
    MmApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(app->screen == MmScreenEdit) {
        mm_draw_editor(canvas, app);
    } else {
        MmDate today;
        MmLifeStats stats;
        mm_today(&today);
        mm_life_stats(&app->dob, &today, &stats);
        if(app->screen == MmScreenGrid) {
            mm_draw_grid(canvas, app, &stats);
        } else {
            mm_draw_stats(canvas, &stats);
        }
    }

    furi_mutex_release(app->mutex);
}

static void mm_input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

/* ---------------------------------------------------------------- input */

static void mm_edit_step(MmApp* app, int delta) {
    MmDate today;
    mm_today(&today);
    MmDate* d = &app->edit;

    switch(app->field) {
    case MmFieldYear: {
        int y = (int)d->year + delta;
        if(y < MM_MIN_YEAR) y = today.year;
        if(y > today.year) y = MM_MIN_YEAR;
        d->year = (uint16_t)y;
        break;
    }
    case MmFieldMonth: {
        int m = (int)d->month + delta;
        if(m < 1) m = 12;
        if(m > 12) m = 1;
        d->month = (uint8_t)m;
        break;
    }
    case MmFieldDay: {
        int dim = mm_days_in_month(d->year, d->month);
        int dd = (int)d->day + delta;
        if(dd < 1) dd = dim;
        if(dd > dim) dd = 1;
        d->day = (uint8_t)dd;
        break;
    }
    default:
        break;
    }

    /* Keep the day valid after a year or month change. */
    uint8_t dim = mm_days_in_month(d->year, d->month);
    if(d->day > dim) d->day = dim;
}

/** Returns false when the app should exit. */
static bool mm_handle_input(MmApp* app, const InputEvent* event) {
    bool is_press = event->type == InputTypeShort;
    bool is_step = is_press || event->type == InputTypeRepeat;

    if(app->screen == MmScreenEdit) {
        if(event->key == InputKeyUp && is_step) {
            mm_edit_step(app, event->type == InputTypeRepeat && app->field == MmFieldYear ? 5 : 1);
        } else if(event->key == InputKeyDown && is_step) {
            mm_edit_step(app, event->type == InputTypeRepeat && app->field == MmFieldYear ? -5 : -1);
        } else if(event->key == InputKeyLeft && is_press) {
            app->field = (app->field + MmFieldCount - 1) % MmFieldCount;
        } else if(event->key == InputKeyRight && is_press) {
            app->field = (app->field + 1) % MmFieldCount;
        } else if(event->key == InputKeyOk && is_press) {
            MmDate today;
            mm_today(&today);
            if(mm_date_compare(&app->edit, &today) > 0) app->edit = today;
            app->dob = app->edit;
            app->has_dob = true;
            mm_save_dob(&app->dob);
            app->screen = app->return_screen;
        } else if(event->key == InputKeyBack && is_press) {
            if(!app->has_dob) return false;
            app->screen = app->return_screen;
        }
        return true;
    }

    if(!is_press) return true;
    switch(event->key) {
    case InputKeyLeft:
    case InputKeyRight:
        app->screen = (app->screen == MmScreenGrid) ? MmScreenStats : MmScreenGrid;
        break;
    case InputKeyOk:
        mm_enter_editor(app);
        break;
    case InputKeyBack:
        return false;
    default:
        break;
    }
    return true;
}

/* ---------------------------------------------------------------- entry */

int32_t momento_mori_app(void* p) {
    UNUSED(p);

    MmApp* app = malloc(sizeof(MmApp));
    memset(app, 0, sizeof(MmApp));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->screen = MmScreenGrid;
    app->has_dob = mm_load_dob(&app->dob);
    if(!app->has_dob) mm_enter_editor(app);

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, mm_draw_callback, app);
    view_port_input_callback_set(view_port, mm_input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    bool running = true;
    InputEvent event;
    while(running) {
        FuriStatus status = furi_message_queue_get(queue, &event, MM_TICK_MS);
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(status == FuriStatusOk) {
            running = mm_handle_input(app, &event);
        } else {
            app->blink = !app->blink;
        }
        furi_mutex_release(app->mutex);
        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
