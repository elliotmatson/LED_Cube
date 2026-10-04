#include <unity.h>

#include <bounce.h>

static const int32_t MAX_X = 100 * 256;
static const int32_t MAX_Y = 50 * 256;

void setUp(void) {}
void tearDown(void) {}

void test_moves_by_velocity_times_time(void)
{
    bounce::Body b;
    b.x = 10 * 256;
    b.y = 10 * 256;
    b.vx = 20 * 256; // 20 px/s
    b.vy = -10 * 256;
    bounce::Hits h = bounce::step(b, MAX_X, MAX_Y, 500);
    TEST_ASSERT_EQUAL_INT32(20 * 256, b.x);
    TEST_ASSERT_EQUAL_INT32(5 * 256, b.y);
    TEST_ASSERT_FALSE(h.x);
    TEST_ASSERT_FALSE(h.y);
}

void test_reflects_off_the_right_wall(void)
{
    bounce::Body b;
    b.x = 98 * 256;
    b.y = 10 * 256;
    b.vx = 4 * 256;
    bounce::Hits h = bounce::step(b, MAX_X, MAX_Y, 1000);
    TEST_ASSERT_EQUAL_INT32(98 * 256, b.x); // 2 past the wall, folded back
    TEST_ASSERT_EQUAL_INT32(-4 * 256, b.vx);
    TEST_ASSERT_TRUE(h.x);
    TEST_ASSERT_FALSE(h.y);
    TEST_ASSERT_FALSE(h.corner());
}

void test_reflects_off_the_top_wall(void)
{
    bounce::Body b;
    b.x = 10 * 256;
    b.y = 1 * 256;
    b.vy = -3 * 256;
    bounce::Hits h = bounce::step(b, MAX_X, MAX_Y, 1000);
    TEST_ASSERT_EQUAL_INT32(2 * 256, b.y);
    TEST_ASSERT_EQUAL_INT32(3 * 256, b.vy);
    TEST_ASSERT_TRUE(h.y);
}

void test_hitting_both_walls_in_one_step_is_a_corner(void)
{
    bounce::Body b;
    b.x = 99 * 256;
    b.y = 49 * 256;
    b.vx = 2 * 256;
    b.vy = 2 * 256;
    bounce::Hits h = bounce::step(b, MAX_X, MAX_Y, 1000);
    TEST_ASSERT_TRUE(h.corner());
    TEST_ASSERT_EQUAL_INT32(-2 * 256, b.vx);
    TEST_ASSERT_EQUAL_INT32(-2 * 256, b.vy);
}

void test_a_long_step_stays_inside(void)
{
    bounce::Body b;
    b.vx = 1000 * 256;
    b.vy = 777 * 256;
    for (int i = 0; i < 50; i++)
    {
        bounce::step(b, MAX_X, MAX_Y, 1234);
        TEST_ASSERT_TRUE(b.x >= 0 && b.x <= MAX_X);
        TEST_ASSERT_TRUE(b.y >= 0 && b.y <= MAX_Y);
    }
}

void test_a_room_no_bigger_than_the_body_pins_it(void)
{
    bounce::Body b;
    b.vx = 5 * 256;
    bounce::step(b, 0, MAX_Y, 1000);
    TEST_ASSERT_EQUAL_INT32(0, b.x);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_moves_by_velocity_times_time);
    RUN_TEST(test_reflects_off_the_right_wall);
    RUN_TEST(test_reflects_off_the_top_wall);
    RUN_TEST(test_hitting_both_walls_in_one_step_is_a_corner);
    RUN_TEST(test_a_long_step_stays_inside);
    RUN_TEST(test_a_room_no_bigger_than_the_body_pins_it);
    return UNITY_END();
}
