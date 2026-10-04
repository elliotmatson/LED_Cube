#include <unity.h>
#include <string.h>

#include <weather.h>

void setUp(void) {}
void tearDown(void) {}

void test_maps_codes_to_skies(void)
{
    TEST_ASSERT_TRUE(weather::sky(0) == weather::Sky::CLEAR);
    TEST_ASSERT_TRUE(weather::sky(2) == weather::Sky::PARTLY_CLOUDY);
    TEST_ASSERT_TRUE(weather::sky(3) == weather::Sky::CLOUDY);
    TEST_ASSERT_TRUE(weather::sky(45) == weather::Sky::FOG);
    TEST_ASSERT_TRUE(weather::sky(53) == weather::Sky::DRIZZLE);
    TEST_ASSERT_TRUE(weather::sky(81) == weather::Sky::RAIN);
    TEST_ASSERT_TRUE(weather::sky(75) == weather::Sky::SNOW);
    TEST_ASSERT_TRUE(weather::sky(99) == weather::Sky::STORM);
    TEST_ASSERT_TRUE(weather::sky(42) == weather::Sky::CLOUDY); // unknown
}

void test_descriptions_fit_one_line_of_the_top_face(void)
{
    for (int code = 0; code < 100; code++)
    {
        const char *d = weather::describe(code);
        TEST_ASSERT_NOT_NULL(d);
        TEST_ASSERT_TRUE(strlen(d) > 0 && strlen(d) <= 10);
    }
    TEST_ASSERT_EQUAL_STRING("Heavy rain", weather::describe(65));
}

void test_intensity_follows_the_code(void)
{
    TEST_ASSERT_EQUAL_FLOAT(0, weather::intensity(0));
    TEST_ASSERT_EQUAL_FLOAT(0.3f, weather::intensity(61));
    TEST_ASSERT_EQUAL_FLOAT(0.6f, weather::intensity(63));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, weather::intensity(65));
}

void test_converts_units(void)
{
    TEST_ASSERT_EQUAL_INT(72, weather::temperature(22.2f, false));
    TEST_ASSERT_EQUAL_INT(22, weather::temperature(22.2f, true));
    TEST_ASSERT_EQUAL_INT(-40, weather::temperature(-40, false));
    TEST_ASSERT_EQUAL_INT(10, weather::windSpeed(16.1f, false));
    TEST_ASSERT_EQUAL_INT(16, weather::windSpeed(16.1f, true));
}

void test_url_encodes_place_names(void)
{
    char out[64];
    TEST_ASSERT_EQUAL_INT(10, weather::urlEncode("New York", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("New%20York", out);
    weather::urlEncode("S\xc3\xa3o Paulo", out, sizeof(out)); // UTF-8 bytes too
    TEST_ASSERT_EQUAL_STRING("S%C3%A3o%20Paulo", out);
}

void test_url_encode_reports_overflow(void)
{
    char out[4];
    TEST_ASSERT_EQUAL_INT(-1, weather::urlEncode("a b", out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(3, weather::urlEncode("abc", out, sizeof(out)));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_maps_codes_to_skies);
    RUN_TEST(test_descriptions_fit_one_line_of_the_top_face);
    RUN_TEST(test_intensity_follows_the_code);
    RUN_TEST(test_converts_units);
    RUN_TEST(test_url_encodes_place_names);
    RUN_TEST(test_url_encode_reports_overflow);
    return UNITY_END();
}
