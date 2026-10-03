#include <unity.h>

#include <rubiks.h>

using namespace rubiks;

// The pattern scrambles, then plays the scramble back inverted; if the model
// were wrong it would never come back to solved. These pin down that, and
// the properties a real cube has.

void setUp(void) {}
void tearDown(void) {}

static uint32_t lcg()
{
    static uint32_t state = 12345;
    state = state * 1103515245u + 12345u;
    return state >> 8;
}

static bool sameState(const Cube &a, const Cube &b)
{
    const int8_t dirs[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (auto &n : dirs)
    {
        for (int8_t i = -1; i <= 1; i++)
            for (int8_t j = -1; j <= 1; j++)
            {
                int8_t pos[3];
                const int axis = n[0] ? 0 : (n[1] ? 1 : 2);
                pos[axis] = n[axis];
                pos[(axis + 1) % 3] = i;
                pos[(axis + 2) % 3] = j;
                if (a.at(pos, n) != b.at(pos, n))
                    return false;
            }
    }
    return true;
}

void test_a_new_cube_is_solved(void)
{
    Cube c;
    TEST_ASSERT_TRUE(c.solved());
}

void test_one_turn_unsolves_it(void)
{
    Cube c;
    c.apply({2, 1, 1});
    TEST_ASSERT_FALSE(c.solved());
}

void test_four_quarter_turns_are_the_identity(void)
{
    for (uint8_t axis = 0; axis < 3; axis++)
        for (int8_t layer = -1; layer <= 1; layer++)
        {
            Cube c, start;
            c.apply({0, 1, 1}); // something not solved
            start = c;
            for (int i = 0; i < 4; i++)
                c.apply({axis, layer, 1});
            TEST_ASSERT_TRUE(sameState(c, start));
        }
}

void test_a_move_then_its_inverse_is_the_identity(void)
{
    Cube c, start;
    Move m{1, -1, 1};
    c.apply(m);
    c.apply(m.inverse());
    TEST_ASSERT_TRUE(sameState(c, start));
}

void test_playing_a_scramble_backwards_solves_it(void)
{
    Cube c;
    Move moves[40];
    for (int i = 0; i < 40; i++)
    {
        moves[i] = randomMove(i ? &moves[i - 1] : nullptr, lcg);
        c.apply(moves[i]);
    }
    TEST_ASSERT_FALSE(c.solved());
    for (int i = 39; i >= 0; i--)
        c.apply(moves[i].inverse());
    TEST_ASSERT_TRUE(c.solved());
}

void test_a_scramble_keeps_nine_stickers_of_each_colour_and_centres_fixed(void)
{
    Cube c;
    Move prev{};
    for (int i = 0; i < 30; i++)
    {
        prev = randomMove(i ? &prev : nullptr, lcg);
        c.apply(prev);
    }
    int counts[6] = {0};
    for (int i = 0; i < 54; i++)
        counts[c.stickers()[i].color]++;
    for (int k = 0; k < 6; k++)
        TEST_ASSERT_EQUAL_INT(9, counts[k]);
    // Outer-layer moves never move a centre.
    const int8_t up[3] = {0, 0, 1}, upCentre[3] = {0, 0, 1};
    TEST_ASSERT_EQUAL(White, c.at(upCentre, up));
}

void test_random_moves_never_undo_the_previous_one(void)
{
    Move prev = randomMove(nullptr, lcg);
    for (int i = 0; i < 500; i++)
    {
        Move m = randomMove(&prev, lcg);
        TEST_ASSERT_FALSE(m.axis == prev.axis && m.layer == prev.layer);
        TEST_ASSERT_TRUE(m.layer == 1 || m.layer == -1);
        prev = m;
    }
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_new_cube_is_solved);
    RUN_TEST(test_one_turn_unsolves_it);
    RUN_TEST(test_four_quarter_turns_are_the_identity);
    RUN_TEST(test_a_move_then_its_inverse_is_the_identity);
    RUN_TEST(test_playing_a_scramble_backwards_solves_it);
    RUN_TEST(test_a_scramble_keeps_nine_stickers_of_each_colour_and_centres_fixed);
    RUN_TEST(test_random_moves_never_undo_the_previous_one);
    return UNITY_END();
}
