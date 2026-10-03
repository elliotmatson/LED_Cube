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

// stepGraph() with a torus neighbourhood must agree with step() exactly.
void test_graph_step_matches_the_grid_step_on_a_torus(void)
{
    static int16_t torus[W * H * 8];
    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            int k = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    if (dx || dy)
                        torus[(y * W + x) * 8 + k++] = ((y + dy + H) % H) * W + (x + dx + W) % W;
        }
    }
    unsigned seed = 12345;
    for (int i = 0; i < W * H; i++)
    {
        seed = seed * 1103515245u + 12345u;
        a[i] = (seed >> 16) % 3 == 0;
    }
    uint8_t viaGraph[W * H];
    for (int generation = 0; generation < 20; generation++)
    {
        int n1 = life::step(a, b, W, H);
        int n2 = life::stepGraph(a, viaGraph, W * H, torus);
        TEST_ASSERT_EQUAL_INT(n1, n2);
        TEST_ASSERT_EQUAL_MEMORY(b, viaGraph, sizeof(viaGraph));
        memcpy(a, b, sizeof(a));
    }
}

void test_missing_neighbours_count_as_dead(void)
{
    // Three cells in a line, each other's only neighbours: the middle one has
    // two live neighbours and survives; the ends have one and die.
    int16_t line[3 * 8];
    for (int i = 0; i < 3 * 8; i++)
        line[i] = -1;
    line[0 * 8] = 1;
    line[1 * 8] = 0;
    line[1 * 8 + 1] = 2;
    line[2 * 8] = 1;
    uint8_t cur[3] = {1, 1, 1}, nxt[3];
    TEST_ASSERT_EQUAL_INT(1, life::stepGraph(cur, nxt, 3, line));
    TEST_ASSERT_EQUAL_UINT8(0, nxt[0]);
    TEST_ASSERT_EQUAL_UINT8(1, nxt[1]);
    TEST_ASSERT_EQUAL_UINT8(0, nxt[2]);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_block_is_a_still_life);
    RUN_TEST(test_blinker_oscillates_with_period_two);
    RUN_TEST(test_glider_moves_one_cell_diagonally_every_four_generations);
    RUN_TEST(test_neighbours_wrap_around_the_edges);
    RUN_TEST(test_every_cell_of_next_is_written);
    RUN_TEST(test_graph_step_matches_the_grid_step_on_a_torus);
    RUN_TEST(test_missing_neighbours_count_as_dead);
    return UNITY_END();
}
