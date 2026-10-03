#include <unity.h>
#include <string.h>

#include <sand.h>

static const int W = 9;
static const int H = 8;
static uint8_t grid[W * H];

static uint32_t lcg()
{
    static uint32_t s = 1;
    s = s * 1103515245u + 12345u;
    return s >> 8;
}

static uint8_t &at(int x, int y) { return grid[y * W + x]; }

static void settle()
{
    for (int i = 0; i < 1000 && sand::step(grid, W, H, lcg); i++)
    {
    }
}

void setUp(void) { memset(grid, 0, sizeof(grid)); }
void tearDown(void) {}

void test_a_grain_falls_one_row_per_step_to_the_bottom(void)
{
    at(4, 7) = 5;
    sand::step(grid, W, H, lcg);
    TEST_ASSERT_EQUAL_UINT8(5, at(4, 6));
    settle();
    TEST_ASSERT_EQUAL_UINT8(5, at(4, 0));
}

void test_grains_keep_their_value(void)
{
    at(2, 5) = 42;
    settle();
    TEST_ASSERT_EQUAL_UINT8(42, at(2, 0));
}

void test_grains_on_a_grain_slide_off_to_the_side(void)
{
    at(4, 0) = 1;
    at(4, 1) = 2;
    sand::step(grid, W, H, lcg);
    TEST_ASSERT_EQUAL_UINT8(0, at(4, 1));
    TEST_ASSERT_TRUE(at(3, 0) == 2 || at(5, 0) == 2);
}

void test_a_poured_column_settles_into_a_pile(void)
{
    for (int y = 0; y < H; y++)
        at(4, y) = 1;
    settle();
    // Nothing can move any more, and no grain sits above an empty cell or
    // beside an empty diagonal below it.
    TEST_ASSERT_EQUAL_INT(0, sand::step(grid, W, H, lcg));
    for (int y = 1; y < H; y++)
        for (int x = 0; x < W; x++)
            if (at(x, y))
                TEST_ASSERT_TRUE(at(x, y - 1) != 0);
}

void test_walls_hold_the_sand(void)
{
    at(0, 3) = 1;
    at(W - 1, 3) = 1;
    settle();
    TEST_ASSERT_EQUAL_UINT8(1, at(0, 0));
    TEST_ASSERT_EQUAL_UINT8(1, at(W - 1, 0));
}

void test_sand_is_never_created_or_lost(void)
{
    for (int i = 0; i < W * H; i++)
        grid[i] = (lcg() % 3 == 0) ? 7 : 0;
    const int before = sand::count(grid, W * H);
    settle();
    TEST_ASSERT_EQUAL_INT(before, sand::count(grid, W * H));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_grain_falls_one_row_per_step_to_the_bottom);
    RUN_TEST(test_grains_keep_their_value);
    RUN_TEST(test_grains_on_a_grain_slide_off_to_the_side);
    RUN_TEST(test_a_poured_column_settles_into_a_pile);
    RUN_TEST(test_walls_hold_the_sand);
    RUN_TEST(test_sand_is_never_created_or_lost);
    return UNITY_END();
}
