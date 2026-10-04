#include <unity.h>

#include <maze.h>

// A plain W x H grid: up, right, down, left.
static const int W = 7, H = 5, N = W * H;
static int16_t nbr[N * 4];
static uint8_t open[N], visited[N];
static maze::Carve events[N];
static int16_t stack[N], dist[N], prev[N], queue[N], path[N];

static void buildGrid()
{
    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            const int c = y * W + x;
            nbr[c * 4 + 0] = y > 0 ? int16_t(c - W) : -1;
            nbr[c * 4 + 1] = x < W - 1 ? int16_t(c + 1) : -1;
            nbr[c * 4 + 2] = y < H - 1 ? int16_t(c + W) : -1;
            nbr[c * 4 + 3] = x > 0 ? int16_t(c - 1) : -1;
        }
    }
}

void setUp(void) { buildGrid(); }
void tearDown(void) {}

void test_generates_a_spanning_tree(void)
{
    uint32_t seed = 12345;
    const int carved = maze::generate(nbr, N, 0, open, events, stack, visited, seed);
    TEST_ASSERT_EQUAL_INT(N - 1, carved); // a tree on N cells has N - 1 edges
    // Every cell reachable.
    maze::search(nbr, open, N, 0, dist, prev, queue);
    for (int i = 0; i < N; i++)
    {
        TEST_ASSERT_TRUE(dist[i] >= 0);
    }
}

void test_passages_are_open_from_both_ends(void)
{
    uint32_t seed = 99;
    maze::generate(nbr, N, 3, open, events, stack, visited, seed);
    int ends = 0;
    for (int c = 0; c < N; c++)
    {
        for (int d = 0; d < 4; d++)
        {
            if (open[c] & (1 << d))
            {
                ends++;
                const int to = nbr[c * 4 + d];
                TEST_ASSERT_TRUE(to >= 0);
                const int back = (d + 2) % 4; // grid directions are opposite
                TEST_ASSERT_TRUE(open[to] & (1 << back));
            }
        }
    }
    TEST_ASSERT_EQUAL_INT(2 * (N - 1), ends);
}

void test_route_walks_open_passages_from_start_to_the_farthest_cell(void)
{
    uint32_t seed = 7;
    maze::generate(nbr, N, 0, open, events, stack, visited, seed);
    const int far = maze::search(nbr, open, N, 0, dist, prev, queue);
    const int n = maze::route(prev, far, path, N);
    TEST_ASSERT_EQUAL_INT(dist[far] + 1, n);
    TEST_ASSERT_EQUAL_INT(0, path[0]);
    TEST_ASSERT_EQUAL_INT(far, path[n - 1]);
    for (int i = 0; i + 1 < n; i++)
    {
        bool joined = false;
        for (int d = 0; d < 4; d++)
        {
            joined |= (open[path[i]] & (1 << d)) && nbr[path[i] * 4 + d] == path[i + 1];
        }
        TEST_ASSERT_TRUE(joined);
    }
    // Nothing is farther from the start.
    for (int i = 0; i < N; i++)
    {
        TEST_ASSERT_TRUE(dist[i] <= dist[far]);
    }
}

void test_different_seeds_give_different_mazes(void)
{
    uint32_t a = 1, b = 2;
    static uint8_t first[N];
    maze::generate(nbr, N, 0, open, events, stack, visited, a);
    for (int i = 0; i < N; i++)
    {
        first[i] = open[i];
    }
    maze::generate(nbr, N, 0, open, events, stack, visited, b);
    bool differs = false;
    for (int i = 0; i < N; i++)
    {
        differs |= first[i] != open[i];
    }
    TEST_ASSERT_TRUE(differs);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_generates_a_spanning_tree);
    RUN_TEST(test_passages_are_open_from_both_ends);
    RUN_TEST(test_route_walks_open_passages_from_start_to_the_farthest_cell);
    RUN_TEST(test_different_seeds_give_different_mazes);
    return UNITY_END();
}
