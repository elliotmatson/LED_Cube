#include <unity.h>
#include <math.h>

#include <pong.h>

static const pong::Court COURT = {128, 64, 2, 2, 12, 0.8f, 1.05f};

void setUp(void) {}
void tearDown(void) {}

void test_bounces_off_the_walls(void)
{
    pong::Ball b = {60, 1, 10, -4};
    const pong::Event e = pong::step(COURT, b, 0, 0, 0.5f); // y would be -1
    TEST_ASSERT_TRUE(e == pong::Event::WALL);
    TEST_ASSERT_EQUAL_FLOAT(1, b.y);
    TEST_ASSERT_EQUAL_FLOAT(4, b.vy);
}

void test_a_square_hit_comes_straight_back_faster(void)
{
    // Ball centred on the left paddle (top at 26, so centre 32).
    pong::Ball b = {3, 31, -20, 0};
    const pong::Event e = pong::step(COURT, b, 26, 0, 0.1f);
    TEST_ASSERT_TRUE(e == pong::Event::LEFT_HIT);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 21, b.vx); // 20 * 1.05, now going right
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0, b.vy);
    TEST_ASSERT_TRUE(b.x >= COURT.paddleWidth);
}

void test_a_tip_hit_comes_back_steep(void)
{
    // Ball at the right paddle's bottom tip.
    pong::Ball b = {123.5f, 37, 20, 0};
    const pong::Event e = pong::step(COURT, b, 0, 26, 0.1f);
    TEST_ASSERT_TRUE(e == pong::Event::RIGHT_HIT);
    TEST_ASSERT_TRUE(b.vx < 0);
    TEST_ASSERT_TRUE(b.vy > 10); // steeply down
}

void test_a_miss_scores_for_the_other_side(void)
{
    pong::Ball b = {3, 5, -20, 0};
    // Paddle far away: the ball goes by.
    TEST_ASSERT_TRUE(pong::step(COURT, b, 40, 0, 0.1f) == pong::Event::NONE);
    pong::Event e = pong::Event::NONE;
    for (int i = 0; i < 10 && e == pong::Event::NONE; i++)
    {
        e = pong::step(COURT, b, 40, 0, 0.1f);
    }
    TEST_ASSERT_TRUE(e == pong::Event::LEFT_MISS);
}

void test_predicts_through_wall_bounces(void)
{
    // From y 10 going down at 1:1, 100 px across: 10 + 100 = 110, which
    // folds off the bottom (62) to 14.
    const pong::Ball b = {10, 10, 50, 50};
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 14, pong::predictY(COURT, b, 110));
    // Moving away: no prediction, current y.
    TEST_ASSERT_EQUAL_FLOAT(10, pong::predictY(COURT, b, 0));
}

void test_prediction_matches_the_simulation(void)
{
    pong::Ball b = {20, 7, 37, -23};
    const float predicted = pong::predictY(COURT, b, 100);
    // Paddles out of the way (no collisions before x = 100).
    while (b.x < 100)
    {
        pong::step(COURT, b, 200, 200, 0.001f);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.2f, predicted, b.y);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_bounces_off_the_walls);
    RUN_TEST(test_a_square_hit_comes_straight_back_faster);
    RUN_TEST(test_a_tip_hit_comes_back_steep);
    RUN_TEST(test_a_miss_scores_for_the_other_side);
    RUN_TEST(test_predicts_through_wall_bounces);
    RUN_TEST(test_prediction_matches_the_simulation);
    return UNITY_END();
}
