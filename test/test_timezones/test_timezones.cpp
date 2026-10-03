#include <unity.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <timezones.h>

// The firmware hands these strings to newlib's localtime(); the host libc
// reads POSIX TZ strings the same way, so a typo in a rule shows up here as a
// wrong offset rather than as a clock that is an hour out for half the year.

void setUp(void) {}
void tearDown(void) {}

// Seconds east of UTC at `when` in `posix`.
static long offsetAt(const char *posix, time_t when)
{
    setenv("TZ", posix, 1);
    tzset();
    struct tm local;
    localtime_r(&when, &local);
    struct tm utc;
    gmtime_r(&when, &utc);
    // Difference between the two broken-down times, as seconds.
    long diff = (local.tm_hour - utc.tm_hour) * 3600L + (local.tm_min - utc.tm_min) * 60L;
    int dayDiff = local.tm_yday - utc.tm_yday;
    if (dayDiff > 1 || dayDiff < -1)
    {
        dayDiff = dayDiff > 0 ? -1 : 1; // across a year boundary
    }
    return diff + dayDiff * 86400L;
}

static const time_t JAN_15_2026 = 1768478400; // 12:00 UTC
static const time_t JUL_15_2026 = 1784116800; // 12:00 UTC

static void expectOffsets(const char *name, long januaryHours, long julyHours, long minutes = 0)
{
    const timezones::Zone *zone = timezones::find(name);
    TEST_ASSERT_NOT_NULL_MESSAGE(zone, name);
    long sign = januaryHours < 0 ? -1 : 1;
    TEST_ASSERT_EQUAL_INT32_MESSAGE(januaryHours * 3600 + sign * minutes * 60, offsetAt(zone->posix, JAN_15_2026), name);
    TEST_ASSERT_EQUAL_INT32_MESSAGE(julyHours * 3600 + sign * minutes * 60, offsetAt(zone->posix, JUL_15_2026), name);
}

void test_offsets_in_winter_and_summer(void)
{
    expectOffsets("UTC", 0, 0);
    expectOffsets("US Eastern", -5, -4);
    expectOffsets("US Central", -6, -5);
    expectOffsets("US Mountain", -7, -6);
    expectOffsets("US Arizona", -7, -7);
    expectOffsets("US Pacific", -8, -7);
    expectOffsets("US Alaska", -9, -8);
    expectOffsets("US Hawaii", -10, -10);
    expectOffsets("Brazil Sao Paulo", -3, -3);
    expectOffsets("UK and Ireland", 0, 1);
    expectOffsets("Central Europe", 1, 2);
    expectOffsets("Eastern Europe", 2, 3);
    expectOffsets("India", 5, 5, 30);
    expectOffsets("China", 8, 8);
    expectOffsets("Japan", 9, 9);
    expectOffsets("Australia Eastern", 11, 10); // southern hemisphere: summer in January
    expectOffsets("New Zealand", 13, 12);
}

void test_us_daylight_saving_starts_on_the_second_sunday_of_march(void)
{
    const char *central = timezones::find("US Central")->posix;
    // 2026-03-08 is the second Sunday; the change is at 02:00 local (08:00 UTC).
    TEST_ASSERT_EQUAL_INT32(-6 * 3600, offsetAt(central, 1772956800 - 1)); // 07:59:59 UTC
    TEST_ASSERT_EQUAL_INT32(-5 * 3600, offsetAt(central, 1772956800));     // 08:00:00 UTC
}

void test_every_zone_has_a_unique_name_without_commas(void)
{
    for (size_t i = 0; i < timezones::ZONE_COUNT; i++)
    {
        TEST_ASSERT_NULL_MESSAGE(strchr(timezones::ZONES[i].name, ','), timezones::ZONES[i].name);
        TEST_ASSERT_EQUAL_PTR(&timezones::ZONES[i], timezones::find(timezones::ZONES[i].name));
    }
}

void test_dropdown_lists_every_zone_in_order(void)
{
    const char *options = timezones::dropdownOptions();
    const char *cursor = options;
    for (size_t i = 0; i < timezones::ZONE_COUNT; i++)
    {
        const char *found = strstr(cursor, timezones::ZONES[i].name);
        TEST_ASSERT_NOT_NULL_MESSAGE(found, timezones::ZONES[i].name);
        cursor = found + strlen(timezones::ZONES[i].name);
    }
    TEST_ASSERT_EQUAL_STRING("UTC", timezones::DEFAULT_ZONE.name);
}

void test_unknown_names_are_not_found(void)
{
    TEST_ASSERT_NULL(timezones::find("Atlantis"));
    TEST_ASSERT_NULL(timezones::find(""));
    TEST_ASSERT_NULL(timezones::find(nullptr));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_offsets_in_winter_and_summer);
    RUN_TEST(test_us_daylight_saving_starts_on_the_second_sunday_of_march);
    RUN_TEST(test_every_zone_has_a_unique_name_without_commas);
    RUN_TEST(test_dropdown_lists_every_zone_in_order);
    RUN_TEST(test_unknown_names_are_not_found);
    return UNITY_END();
}
