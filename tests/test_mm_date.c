/* Host-side unit tests for the date maths: cc -I.. test_mm_date.c ../mm_date.c && ./a.out */
#include <stdio.h>
#include "../mm_date.h"

static int failures = 0;
#define CHECK_EQ(actual, expected)                                                         \
    do {                                                                                   \
        long _a = (long)(actual), _e = (long)(expected);                                   \
        if(_a != _e) {                                                                     \
            printf("FAIL %s:%d: %s = %ld, expected %ld\n", __FILE__, __LINE__, #actual, _a, _e); \
            failures++;                                                                    \
        }                                                                                  \
    } while(0)

int main(void) {
    MmDate epoch = {1970, 1, 1};
    MmDate y2k = {2000, 3, 1};
    CHECK_EQ(mm_days_from_civil(&epoch), 0);
    CHECK_EQ(mm_days_from_civil(&y2k), 11017);
    MmDate pre = {1900, 1, 1};
    CHECK_EQ(mm_days_from_civil(&pre), -25567);

    CHECK_EQ(mm_days_in_month(2000, 2), 29);
    CHECK_EQ(mm_days_in_month(1900, 2), 28);
    CHECK_EQ(mm_days_in_month(2024, 2), 29);
    CHECK_EQ(mm_days_in_month(2023, 4), 30);

    MmDate bad = {2023, 2, 29};
    CHECK_EQ(mm_date_is_valid(&bad), 0);

    MmLifeStats s;
    MmDate dob = {1990, 1, 1};
    MmDate today = {2026, 11, 16};
    mm_life_stats(&dob, &today, &s);
    CHECK_EQ(s.days_lived, 13468);
    CHECK_EQ(s.weeks_lived, 1924);
    CHECK_EQ(s.weeks_left, 2076);
    CHECK_EQ(s.permille_lived, 481);
    CHECK_EQ(s.age_years, 36);
    CHECK_EQ(s.age_weeks, 45);

    /* Day before birthday. */
    MmDate dob2 = {2000, 11, 17};
    mm_life_stats(&dob2, &today, &s);
    CHECK_EQ(s.age_years, 25);
    CHECK_EQ(s.age_weeks, 52);

    /* Leap-day birthday in a common year. */
    MmDate leap = {2000, 2, 29};
    MmDate feb28 = {2001, 2, 28};
    mm_life_stats(&leap, &feb28, &s);
    CHECK_EQ(s.age_years, 1);
    CHECK_EQ(s.age_weeks, 0);

    /* Born today and in the future. */
    mm_life_stats(&today, &today, &s);
    CHECK_EQ(s.weeks_lived, 0);
    CHECK_EQ(s.weeks_left, 4000);
    MmDate future = {2030, 1, 1};
    mm_life_stats(&future, &today, &s);
    CHECK_EQ(s.days_lived, 0);

    /* Past 4000 weeks: bonus time. */
    MmDate old = {1940, 1, 1};
    mm_life_stats(&old, &today, &s);
    CHECK_EQ(s.weeks_left < 0, 1);
    CHECK_EQ(s.permille_lived, 1000);

    if(failures == 0) printf("All tests passed\n");
    return failures ? 1 : 0;
}
