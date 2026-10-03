#include <unity.h>
#include <string.h>

#include <life.h>

// Known Life patterns pin the rules down; the uninitialized-buffer test pins
// down the bug the pattern used to have.

static const int W = 8;
static const int H = 8;
static uint8_t a[W * H];
static uint8_t b[W * H];

void setUp(void)
{
    memset(a, 0, sizeof(a));
    memset(b, 0, sizeof(b));
}
void tearDown(void) {}

static void set(uint8_t *g, int x, int y) { g[y * W + x] = 1; }
static bool alive(const uint8_t *g, int x, int y) { return g[y * W + x] != 0; }

void test_block_is_a_still_life(void)
{
    set(a, 2, 2); set(a, 3, 2); set(a, 2, 3); set(a, 3, 3);
    TEST_ASSERT_EQUAL_INT(4, life::step(a, b, W, H));
    TEST_ASSERT_EQUAL_MEMORY(a, b, sizeof(a));
}

void test_blinker_oscillates_with_period_two(void)
{
    set(a, 3, 2); set(a, 3, 3); set(a, 3, 4); // vertical
    life::step(a, b, W, H);
    TEST_ASSERT_TRUE(alive(b, 2, 3) && alive(b, 3, 3) && alive(b, 4, 3));
    TEST_ASSERT_FALSE(alive(b, 3, 2) || alive(b, 3, 4));
    uint8_t c[W * H];
    life::step(b, c, W, H);
    TEST_ASSERT_EQUAL_MEMORY(a, c, sizeof(a));
}

void test_glider_moves_one_cell_diagonally_every_four_generations(void)
{
    set(a, 1, 0); set(a, 2, 1); set(a, 0, 2); set(a, 1, 2); set(a, 2, 2);
    uint8_t *cur = a, *next = b;
    for (int i = 0; i < 4; i++)
    {
        life::step(cur, next, W, H);
        uint8_t *t = cur; cur = next; next = t;
    }
    uint8_t expected[W * H] = {0};
    set(expected, 2, 1); set(expected, 3, 2); set(expected, 1, 3); set(expected, 2, 3); set(expected, 3, 3);
    TEST_ASSERT_EQUAL_MEMORY(expected, cur, sizeof(expected));
}

void test_neighbours_wrap_around_the_edges(void)
{
    set(a, W - 1, H - 1);
    TEST_ASSERT_EQUAL_INT(1, life::neighbours(a, W, H, 0, 0));
    set(a, 0, H - 1);
    set(a, W - 1, 0);
    TEST_ASSERT_EQUAL_INT(3, life::neighbours(a, W, H, 0, 0));
}

// The pattern used to only write cells that changed, so survivors took
// whatever the next buffer held -- uninitialized memory on the first
// generation.
void test_every_cell_of_next_is_written(void)
{
    set(a, 2, 2); set(a, 3, 2); set(a, 2, 3); set(a, 3, 3);
    memset(b, 0xAA, sizeof(b));
    life::step(a, b, W, H);
    TEST_ASSERT_EQUAL_MEMORY(a, b, sizeof(a));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_block_is_a_still_life);
    RUN_TEST(test_blinker_oscillates_with_period_two);
    RUN_TEST(test_glider_moves_one_cell_diagonally_every_four_generations);
    RUN_TEST(test_neighbours_wrap_around_the_edges);
    RUN_TEST(test_every_cell_of_next_is_written);
    return UNITY_END();
}
