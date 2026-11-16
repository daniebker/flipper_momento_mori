# Memento Mori for Flipper Zero

> "The average human lifespan is absurdly, terrifyingly, insultingly short...
> assuming you live to be eighty, you have just over four thousand weeks."
> — Oliver Burkeman, *Four Thousand Weeks*

A tiny reminder to spend your time well. Enter your date of birth once and the
app shows your life as **4000 weeks**, how many you've lived and how many remain.

## Screens

**1 — The grid.** All 4000 weeks drawn as an 80 × 50 pixel grid, one pixel per
week. Lived weeks are filled in, the week you're living now blinks, and the
empty space is what's left. The side panel shows weeks left, weeks lived and
the share of the 4000 already used.

**2 — The count.** Weeks remaining in big digits, a progress bar across your
4000 weeks, your age in years and weeks, days remaining, and a short
reminder that changes every day.

Past week 4000? The app switches to counting your **bonus weeks**.

## Controls

| Screen      | Button      | Action                         |
|-------------|-------------|--------------------------------|
| Grid / Count| ◀ ▶         | Switch between screens         |
| Grid / Count| OK          | Edit date of birth             |
| Grid / Count| Back        | Exit                           |
| Editor      | ◀ ▶         | Select year / month / day      |
| Editor      | ▲ ▼         | Change value (hold to repeat; year jumps by 5) |
| Editor      | OK          | Save                           |
| Editor      | Back        | Cancel                         |

On first launch the date-of-birth editor opens automatically. Your date of
birth is stored on the SD card at `apps_data/momento_mori/dob.conf`.

The app reads today's date from the Flipper's clock, so make sure it is set
(Settings → System, or sync it with the qFlipper / mobile app).

## Build

Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
pip install ufbt
```

```bash
ufbt
```

```bash
ufbt launch
```

`ufbt` writes `dist/momento_mori.fap`; copy it to `apps/Tools/` on the SD card,
or use `ufbt launch` with the Flipper connected over USB. CI builds the `.fap`
against the release and dev SDKs on every push and uploads it as an artifact.

The date maths has host-side unit tests:

```bash
gcc -Wall -Wextra -o test_mm_date tests/test_mm_date.c mm_date.c && ./test_mm_date
```
