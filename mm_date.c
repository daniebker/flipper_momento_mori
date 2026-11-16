#include "mm_date.h"

bool mm_is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

uint8_t mm_days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if(month < 1 || month > 12) return 0;
    if(month == 2 && mm_is_leap_year(year)) return 29;
    return days[month - 1];
}

bool mm_date_is_valid(const MmDate* date) {
    return date->year >= MM_MIN_YEAR && date->month >= 1 && date->month <= 12 &&
           date->day >= 1 && date->day <= mm_days_in_month(date->year, date->month);
}

int32_t mm_days_from_civil(const MmDate* date) {
    /* Howard Hinnant's days_from_civil algorithm. */
    int32_t y = (int32_t)date->year - (date->month <= 2 ? 1 : 0);
    int32_t m = date->month;
    int32_t d = date->day;
    int32_t era = (y >= 0 ? y : y - 399) / 400;
    int32_t yoe = y - era * 400; /* [0, 399] */
    int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; /* [0, 365] */
    int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; /* [0, 146096] */
    return era * 146097 + doe - 719468;
}

int mm_date_compare(const MmDate* a, const MmDate* b) {
    if(a->year != b->year) return a->year < b->year ? -1 : 1;
    if(a->month != b->month) return a->month < b->month ? -1 : 1;
    if(a->day != b->day) return a->day < b->day ? -1 : 1;
    return 0;
}

/* Birthday in a given year; Feb 29 birthdays fall on Feb 28 in common years. */
static MmDate mm_birthday_in(const MmDate* dob, uint16_t year) {
    MmDate b = {.year = year, .month = dob->month, .day = dob->day};
    uint8_t dim = mm_days_in_month(year, b.month);
    if(b.day > dim) b.day = dim;
    return b;
}

void mm_life_stats(const MmDate* dob, const MmDate* today, MmLifeStats* out) {
    int32_t days = mm_days_from_civil(today) - mm_days_from_civil(dob);
    if(days < 0) days = 0;

    out->days_lived = days;
    out->weeks_lived = days / 7;
    out->weeks_left = MM_LIFE_WEEKS - out->weeks_lived;
    out->days_left = MM_LIFE_WEEKS * 7 - days;

    int32_t permille = (out->weeks_lived * 1000) / MM_LIFE_WEEKS;
    out->permille_lived = (uint16_t)(permille > 1000 ? 1000 : permille);

    if(mm_date_compare(today, dob) <= 0) {
        out->age_years = 0;
        out->age_weeks = 0;
        return;
    }

    uint16_t years = today->year - dob->year;
    MmDate last_birthday = mm_birthday_in(dob, today->year);
    if(mm_date_compare(today, &last_birthday) < 0) {
        years--;
        last_birthday = mm_birthday_in(dob, today->year - 1);
    }
    out->age_years = years;
    out->age_weeks =
        (uint16_t)((mm_days_from_civil(today) - mm_days_from_civil(&last_birthday)) / 7);
}
