#include <unity.h>

#include <color.h>

using color::RGB;

void setUp(void) {}
void tearDown(void) {}

static void expectRGB(RGB expected, RGB actual, int tolerance)
{
    TEST_ASSERT_INT_WITHIN(tolerance, expected.r, actual.r);
    TEST_ASSERT_INT_WITHIN(tolerance, expected.g, actual.g);
    TEST_ASSERT_INT_WITHIN(tolerance, expected.b, actual.b);
}

void test_hsv_primaries(void)
{
    expectRGB({255, 0, 0}, color::hsv(0, 255, 255), 2);
    expectRGB({0, 255, 0}, color::hsv(85, 255, 255), 8);
    expectRGB({0, 0, 255}, color::hsv(171, 255, 255), 8);
}

void test_hsv_without_saturation_is_grey(void)
{
    expectRGB({100, 100, 100}, color::hsv(123, 0, 100), 0);
}

void test_hsv_value_scales_brightness(void)
{
    RGB dim = color::hsv(40, 255, 64);
    TEST_ASSERT_TRUE(dim.r <= 64 && dim.g <= 64 && dim.b <= 64);
}

void test_lerp_ends_and_middle(void)
{
    RGB a{0, 100, 200}, b{200, 100, 0};
    expectRGB(a, color::lerp(a, b, 0), 0);
    expectRGB(b, color::lerp(a, b, 255), 0);
    expectRGB({100, 100, 100}, color::lerp(a, b, 128), 1);
}

void test_gradient_hits_every_stop(void)
{
    const RGB stops[] = {{0, 0, 0}, {255, 0, 0}, {255, 255, 255}};
    expectRGB(stops[0], color::gradient(stops, 3, 0), 0);
    expectRGB(stops[1], color::gradient(stops, 3, 128), 3);
    expectRGB(stops[2], color::gradient(stops, 3, 255), 0);
}

void test_gradient_is_monotonic_between_stops(void)
{
    const RGB stops[] = {{0, 0, 0}, {255, 255, 255}};
    int last = -1;
    for (int t = 0; t < 256; t++)
    {
        int v = color::gradient(stops, 2, uint8_t(t)).r;
        TEST_ASSERT_TRUE(v >= last);
        last = v;
    }
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_hsv_primaries);
    RUN_TEST(test_hsv_without_saturation_is_grey);
    RUN_TEST(test_hsv_value_scales_brightness);
    RUN_TEST(test_lerp_ends_and_middle);
    RUN_TEST(test_gradient_hits_every_stop);
    RUN_TEST(test_gradient_is_monotonic_between_stops);
    return UNITY_END();
}
