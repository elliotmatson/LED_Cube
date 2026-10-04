#include <unity.h>
#include <math.h>
#include <string.h>

#include <breakout.h>

static const breakout::Field F = {128, 64, 2, 60, 14, 2, 1.0f, 1.0f, 16, 6, 4, 8, 3};
static uint8_t bricks[16 * 6];

void setUp(void) { memset(bricks, 1, sizeof(bricks)); }
void tearDown(void) {}

void test_bounces_off_the_side_walls(void)
{
    memset(bricks, 0, sizeof(bricks));
    breakout::Ball b = {1, 40, -10, 0};
    const breakout::Result r = breakout::step(F, b, bricks, 0, 0.2f);
    TEST_ASSERT_TRUE(r.event == breakout::Event::WALL);
    TEST_ASSERT_EQUAL_FLOAT(1, b.x);
    TEST_ASSERT_EQUAL_FLOAT(10, b.vx);
}

void test_breaks_a_brick_and_bounces_back_down(void)
{
    // Rising straight into row 5 (y 19-20), column 3 (x 24-30).
    breakout::Ball b = {26, 23, 0, -20};
    breakout::Result r;
    for (int i = 0; i < 20 && r.event != breakout::Event::BRICK; i++)
    {
        r = breakout::step(F, b, bricks, 0, 0.02f);
    }
    TEST_ASSERT_TRUE(r.event == breakout::Event::BRICK);
    TEST_ASSERT_EQUAL_INT(5 * 16 + 3, r.brick);
    TEST_ASSERT_EQUAL_INT(0, bricks[5 * 16 + 3]);
    TEST_ASSERT_TRUE(b.vy > 0); // heading back down
    TEST_ASSERT_EQUAL_INT(16 * 6 - 1, breakout::remaining(F, bricks));
}

void test_a_side_hit_turns_the_ball_sideways(void)
{
    // Moving right along row 5's height into the left side of column 4.
    memset(bricks, 0, sizeof(bricks));
    bricks[5 * 16 + 4] = 1;
    breakout::Ball b = {28.5f, 19.2f, 20, 0};
    breakout::Result r;
    for (int i = 0; i < 20 && r.event != breakout::Event::BRICK; i++)
    {
        r = breakout::step(F, b, bricks, 0, 0.02f);
    }
    TEST_ASSERT_TRUE(r.event == breakout::Event::BRICK);
    TEST_ASSERT_TRUE(b.vx < 0);
}

void test_the_paddle_returns_the_ball_angled_by_where_it_hit(void)
{
    memset(bricks, 0, sizeof(bricks));
    // Centre hit: straight up.
    breakout::Ball b = {56, 57.5f, 0, 30};
    breakout::Result r = breakout::step(F, b, bricks, 50, 0.05f);
    TEST_ASSERT_TRUE(r.event == breakout::Event::PADDLE);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, b.vx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -30, b.vy);
    // Right tip: up and to the right, same speed.
    breakout::Ball c = {63, 57.5f, 0, 30};
    r = breakout::step(F, c, bricks, 50, 0.05f);
    TEST_ASSERT_TRUE(r.event == breakout::Event::PADDLE);
    TEST_ASSERT_TRUE(c.vx > 10 && c.vy < 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 30, sqrtf(c.vx * c.vx + c.vy * c.vy));
}

void test_missing_the_paddle_is_a_miss(void)
{
    memset(bricks, 0, sizeof(bricks));
    breakout::Ball b = {10, 58, 0, 30};
    breakout::Result r;
    for (int i = 0; i < 30 && r.event != breakout::Event::MISS; i++)
    {
        r = breakout::step(F, b, bricks, 80, 0.05f);
    }
    TEST_ASSERT_TRUE(r.event == breakout::Event::MISS);
}

void test_predicts_the_landing_through_wall_bounces(void)
{
    // 50 px down at 1:1 from x 100 heading right: 150 folds off the right
    // wall (126) to 102.
    const breakout::Ball b = {100, 8, 20, 20};
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 102, breakout::predictX(F, b, 58));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_bounces_off_the_side_walls);
    RUN_TEST(test_breaks_a_brick_and_bounces_back_down);
    RUN_TEST(test_a_side_hit_turns_the_ball_sideways);
    RUN_TEST(test_the_paddle_returns_the_ball_angled_by_where_it_hit);
    RUN_TEST(test_missing_the_paddle_is_a_miss);
    RUN_TEST(test_predicts_the_landing_through_wall_bounces);
    return UNITY_END();
}
