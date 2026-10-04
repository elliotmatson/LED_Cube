#include <unity.h>
#include <string.h>

#include <wordclock.h>

static char buf[96];

static const char *say(int h, int m) { return wordclock::phrase(h, m, buf, sizeof(buf)); }

void setUp(void) {}
void tearDown(void) {}

void test_grid_is_sixteen_by_eight(void)
{
    for (int r = 0; r < wordclock::ROWS; r++)
    {
        TEST_ASSERT_EQUAL_INT(16, (int)strlen(wordclock::GRID[r]));
    }
}

void test_on_the_hour(void)
{
    TEST_ASSERT_EQUAL_STRING("IT IS THREE OCLOCK PM", say(15, 0));
    TEST_ASSERT_EQUAL_STRING("IT IS TWELVE AM OCLOCK", say(0, 2));
}

void test_minutes_past(void)
{
    TEST_ASSERT_EQUAL_STRING("IT IS QUARTER PAST NINE AM", say(9, 17));
    TEST_ASSERT_EQUAL_STRING("IT IS HALF PAST ELEVEN PM", say(23, 34));
    TEST_ASSERT_EQUAL_STRING("IT IS TWENTY FIVE MINUTES PAST SIX PM", say(18, 25));
    TEST_ASSERT_EQUAL_STRING("IT IS TEN MINUTES PAST FOUR AM", say(4, 12));
}

void test_minutes_to_name_the_next_hour(void)
{
    TEST_ASSERT_EQUAL_STRING("IT IS QUARTER TO TEN AM", say(9, 45));
    TEST_ASSERT_EQUAL_STRING("IT IS FIVE MINUTES TO ONE PM", say(12, 58));
    // Twenty to twelve in the morning means noon is coming: PM.
    TEST_ASSERT_EQUAL_STRING("IT IS TWENTY MINUTES TO TWELVE PM", say(11, 40));
    // And twenty to midnight wraps to the morning.
    TEST_ASSERT_EQUAL_STRING("IT IS TWENTY FIVE MINUTES TO TWELVE AM", say(23, 35));
}

void test_lights_exactly_the_phrase(void)
{
    uint8_t lit[16 * 8];
    wordclock::light(9, 45, lit);
    int count = 0;
    for (int i = 0; i < 16 * 8; i++)
    {
        count += lit[i];
    }
    // IT IS QUARTER TO TEN AM: 2 + 2 + 7 + 2 + 3 + 2 letters.
    TEST_ASSERT_EQUAL_INT(18, count);
    TEST_ASSERT_EQUAL_INT(1, lit[0]);  // I
    TEST_ASSERT_EQUAL_INT(0, lit[2]);  // K, filler
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_grid_is_sixteen_by_eight);
    RUN_TEST(test_on_the_hour);
    RUN_TEST(test_minutes_past);
    RUN_TEST(test_minutes_to_name_the_next_hour);
    RUN_TEST(test_lights_exactly_the_phrase);
    return UNITY_END();
}
