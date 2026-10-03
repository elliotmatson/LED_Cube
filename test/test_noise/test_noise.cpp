#include <unity.h>
#include <math.h>

#include <cube_geometry.h>
#include <noise.h>

// Patterns sample this at every pixel's 3D position on the cube; these pin
// down the properties that makes look right: bounded, repeatable, smooth,
// and therefore seamless across the cube's edges.

void setUp(void) {}
void tearDown(void) {}

void test_zero_at_lattice_points(void)
{
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, noise::perlin(0, 0, 0));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, noise::perlin(3, -7, 12));
}

void test_bounded_and_not_flat(void)
{
    float lo = 10, hi = -10;
    for (int i = 0; i < 20000; i++)
    {
        float v = noise::perlin(i * 0.137f, i * 0.071f, i * 0.0313f);
        TEST_ASSERT_TRUE(v >= -1.1f && v <= 1.1f);
        lo = fminf(lo, v);
        hi = fmaxf(hi, v);
    }
    TEST_ASSERT_TRUE(lo < -0.5f);
    TEST_ASSERT_TRUE(hi > 0.5f);
}

void test_deterministic(void)
{
    TEST_ASSERT_EQUAL_FLOAT(noise::perlin(1.3f, 2.7f, 0.4f), noise::perlin(1.3f, 2.7f, 0.4f));
}

void test_fbm_stays_in_range(void)
{
    for (int i = 0; i < 5000; i++)
    {
        float v = noise::fbm(i * 0.21f, i * 0.05f, 1.5f, 4);
        TEST_ASSERT_TRUE(v >= -1.1f && v <= 1.1f);
    }
}

// Neighbouring pixels, including across seams, are at most ~1 unit apart in
// 3D (see test_cube_geometry); at the scales patterns use, the field must
// barely change between them, or an edge would show as a line.
void test_sampled_on_the_cube_it_is_continuous_across_seams(void)
{
    const float SCALE = 1.0f / 16;
    float worst = 0;
    for (int16_t x = 0; x < cube::CHAIN_WIDTH; x++)
    {
        for (int16_t y = 0; y < cube::CHAIN_HEIGHT; y++)
        {
            for (uint8_t d = 0; d < 4; d++)
            {
                cube::Step s = cube::step({x, y}, cube::Dir(d));
                if (!s.valid())
                    continue;
                cube::Vec3 a = cube::toCube({x, y}), b = cube::toCube(s.to);
                float diff = fabsf(noise::fbm(a.x * SCALE, a.y * SCALE, a.z * SCALE, 3) -
                                   noise::fbm(b.x * SCALE, b.y * SCALE, b.z * SCALE, 3));
                worst = fmaxf(worst, diff);
            }
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(worst < 0.25f, "visible step between neighbouring pixels");
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_zero_at_lattice_points);
    RUN_TEST(test_bounded_and_not_flat);
    RUN_TEST(test_deterministic);
    RUN_TEST(test_fbm_stays_in_range);
    RUN_TEST(test_sampled_on_the_cube_it_is_continuous_across_seams);
    return UNITY_END();
}
