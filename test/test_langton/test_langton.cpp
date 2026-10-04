#include <unity.h>
#include <string.h>

#include <langton.h>

// A W x H torus: up, right, down, left.
static const int W = 80, H = 80, N = W * H;
static int16_t nbr[N * 4];
static uint8_t grid[N];

static void buildTorus()
{
    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            const int c = y * W + x;
            nbr[c * 4 + 0] = int16_t(((y + H - 1) % H) * W + x);
            nbr[c * 4 + 1] = int16_t(y * W + (x + 1) % W);
            nbr[c * 4 + 2] = int16_t(((y + 1) % H) * W + x);
            nbr[c * 4 + 3] = int16_t(y * W + (x + W - 1) % W);
        }
    }
}

void setUp(void)
{
    buildTorus();
    memset(grid, 0, sizeof(grid));
}
void tearDown(void) {}

void test_parses_rules(void)
{
    langton::Rule r;
    TEST_ASSERT_TRUE(langton::parse("RL", r));
    TEST_ASSERT_EQUAL_INT(2, r.states);
    TEST_ASSERT_TRUE(langton::parse("LLRRNU", r));
    TEST_ASSERT_FALSE(langton::parse("", r));
    TEST_ASSERT_FALSE(langton::parse("RX", r));
}

void test_classic_ant_draws_a_square_in_four_steps(void)
{
    langton::Rule r;
    langton::parse("RL", r);
    const int start = 40 * W + 40;
    langton::Ant ant{int16_t(start), 0};
    for (int i = 0; i < 4; i++)
    {
        langton::step(r, grid, nbr, ant);
    }
    // Right, down, left, up: a 2x2 square to the start's right and below.
    TEST_ASSERT_EQUAL_INT(1, grid[start]);
    TEST_ASSERT_EQUAL_INT(1, grid[start + 1]);
    TEST_ASSERT_EQUAL_INT(1, grid[start + W + 1]);
    TEST_ASSERT_EQUAL_INT(1, grid[start + W]);
    TEST_ASSERT_EQUAL_INT(start, ant.cell); // back where it began
    TEST_ASSERT_EQUAL_INT(0, ant.dir);
}

void test_classic_ant_builds_its_highway(void)
{
    // After about 10,000 steps of chaos the classic ant settles into a
    // 104-step cycle that moves it diagonally: the highway. Afterwards,
    // every 104 steps it is 2 cells further along both axes.
    langton::Rule r;
    langton::parse("RL", r);
    langton::Ant ant{int16_t(40 * W + 40), 0};
    for (int i = 0; i < 11000; i++)
    {
        langton::step(r, grid, nbr, ant);
    }
    const int x0 = ant.cell % W, y0 = ant.cell / W;
    for (int i = 0; i < 104; i++)
    {
        langton::step(r, grid, nbr, ant);
    }
    const int dx = (ant.cell % W - x0 + W) % W, dy = (ant.cell / W - y0 + H) % H;
    TEST_ASSERT_TRUE(dx == 2 || dx == W - 2);
    TEST_ASSERT_TRUE(dy == 2 || dy == H - 2);
}

void test_turns_round_at_a_wall(void)
{
    // A single row with walls at both ends.
    static int16_t line[3 * 4] = {-1, 1, -1, -1, -1, 2, -1, 0, -1, -1, -1, 1};
    static uint8_t cells[3] = {0, 0, 0};
    langton::Rule r;
    langton::parse("N", r); // straight on, always
    langton::Ant ant{1, 1};
    langton::step(r, cells, line, ant); // to cell 2
    TEST_ASSERT_EQUAL_INT(2, ant.cell);
    langton::step(r, cells, line, ant); // wall: turn round
    TEST_ASSERT_EQUAL_INT(2, ant.cell);
    TEST_ASSERT_EQUAL_INT(3, ant.dir);
}

void test_heading_turns_across_a_rotated_link(void)
{
    // Cell 0's right leads to cell 1, whose way back is its "up": arriving,
    // the ant heads down in cell 1's frame.
    static int16_t g[2 * 4] = {-1, 1, -1, -1, 0, -1, -1, -1};
    static uint8_t cells[2] = {0, 0};
    langton::Rule r;
    langton::parse("N", r);
    langton::Ant ant{0, 1};
    langton::step(r, cells, g, ant);
    TEST_ASSERT_EQUAL_INT(1, ant.cell);
    TEST_ASSERT_EQUAL_INT(2, ant.dir);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_parses_rules);
    RUN_TEST(test_classic_ant_draws_a_square_in_four_steps);
    RUN_TEST(test_classic_ant_builds_its_highway);
    RUN_TEST(test_turns_round_at_a_wall);
    RUN_TEST(test_heading_turns_across_a_rotated_link);
    return UNITY_END();
}
