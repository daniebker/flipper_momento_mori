#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Length of an average life, measured in weeks (Oliver Burkeman, "Four Thousand Weeks"). */
#define MM_LIFE_WEEKS (4000)
#define MM_MIN_YEAR   (1900)

typedef struct {
    uint16_t year;
    uint8_t month; /* 1..12 */
    uint8_t day; /* 1..31 */
} MmDate;

typedef struct {
    int32_t days_lived; /* days since birth, clamped at 0 */
    int32_t weeks_lived; /* completed weeks since birth */
    int32_t weeks_left; /* MM_LIFE_WEEKS - weeks_lived, negative once past 4000 */
    int32_t days_left; /* MM_LIFE_WEEKS * 7 - days_lived, negative once past */
    uint16_t permille_lived; /* 0..1000, share of the 4000 weeks already lived */
    uint16_t age_years; /* completed years */
    uint16_t age_weeks; /* completed weeks since the last birthday */
} MmLifeStats;

bool mm_is_leap_year(uint16_t year);
uint8_t mm_days_in_month(uint16_t year, uint8_t month);
bool mm_date_is_valid(const MmDate* date);

/** Days since 1970-01-01 in the proleptic Gregorian calendar. */
int32_t mm_days_from_civil(const MmDate* date);

/** Negative if a < b, 0 if equal, positive if a > b. */
int mm_date_compare(const MmDate* a, const MmDate* b);

void mm_life_stats(const MmDate* dob, const MmDate* today, MmLifeStats* out);
