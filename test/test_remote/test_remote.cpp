#include <unity.h>
#include <string.h>

#include <remote.h>

static remote::Command cmd;

static const char *parse(const char *name, const char *payload)
{
    return remote::parse(name, payload, payload ? strlen(payload) : 0, cmd);
}

void setUp(void) {}
void tearDown(void) {}

void test_unknown_settings_are_rejected(void)
{
    TEST_ASSERT_EQUAL_STRING("unknown setting", parse("wifi_password", "x"));
    TEST_ASSERT_EQUAL_STRING("unknown setting", parse("", "x"));
    TEST_ASSERT_EQUAL_STRING("unknown setting", parse(nullptr, "x"));
}

void test_numbers_are_checked_against_their_range(void)
{
    TEST_ASSERT_NULL(parse("brightness", "128"));
    TEST_ASSERT_EQUAL_INT(128, cmd.number);
    TEST_ASSERT_TRUE(cmd.id == remote::Setting::BRIGHTNESS);
    TEST_ASSERT_EQUAL_STRING("out of range", parse("brightness", "256"));
    TEST_ASSERT_EQUAL_STRING("out of range", parse("brightness", "-1"));
    TEST_ASSERT_EQUAL_STRING("not a number", parse("brightness", "12abc"));
    TEST_ASSERT_EQUAL_STRING("not a number", parse("brightness", ""));
    TEST_ASSERT_EQUAL_STRING("out of range", parse("telemetry_interval", "10")); // under a minute
    TEST_ASSERT_NULL(parse("telemetry_interval", " 300 "));
    TEST_ASSERT_EQUAL_INT(300, cmd.number);
}

void test_flags_accept_the_usual_spellings(void)
{
    const char *yes[] = {"true", "TRUE", "on", "yes", "1", "\"true\""};
    for (const char *v : yes)
    {
        TEST_ASSERT_NULL(parse("ota", v));
        TEST_ASSERT_TRUE(cmd.flag);
    }
    const char *no[] = {"false", "Off", "no", "0"};
    for (const char *v : no)
    {
        TEST_ASSERT_NULL(parse("ota", v));
        TEST_ASSERT_FALSE(cmd.flag);
    }
    TEST_ASSERT_EQUAL_STRING("not true or false", parse("ota", "maybe"));
}

void test_text_is_unwrapped_and_limited(void)
{
    TEST_ASSERT_NULL(parse("ticker", "\"Hello, cube\""));
    TEST_ASSERT_EQUAL_STRING("Hello, cube", cmd.text.c_str());
    TEST_ASSERT_NULL(parse("weather_location", "  Austin, Texas \n"));
    TEST_ASSERT_EQUAL_STRING("Austin, Texas", cmd.text.c_str());
    TEST_ASSERT_NULL(parse("ticker", "")); // clears it
    TEST_ASSERT_EQUAL_STRING("", cmd.text.c_str());
    char longText[80];
    memset(longText, 'a', sizeof(longText) - 1);
    longText[sizeof(longText) - 1] = '\0';
    TEST_ASSERT_EQUAL_STRING("too long", parse("weather_location", longText));
    const char withNul[] = {'a', '\0', 'b'};
    TEST_ASSERT_EQUAL_STRING("contains a NUL", remote::parse("ticker", withNul, 3, cmd));
}

void test_actions_ignore_their_payload(void)
{
    TEST_ASSERT_NULL(parse("restart", ""));
    TEST_ASSERT_TRUE(cmd.type == remote::Type::ACTION);
    TEST_ASSERT_TRUE(cmd.id == remote::Setting::RESTART);
    TEST_ASSERT_NULL(parse("check_updates", "anything"));
}

void test_every_spec_is_findable(void)
{
    size_t count = 0;
    const remote::Spec *all = remote::specs(count);
    TEST_ASSERT_EQUAL_INT(16, (int)count);
    for (size_t i = 0; i < count; i++)
    {
        TEST_ASSERT_EQUAL_PTR(&all[i], remote::find(all[i].name));
    }
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_unknown_settings_are_rejected);
    RUN_TEST(test_numbers_are_checked_against_their_range);
    RUN_TEST(test_flags_accept_the_usual_spellings);
    RUN_TEST(test_text_is_unwrapped_and_limited);
    RUN_TEST(test_actions_ignore_their_payload);
    RUN_TEST(test_every_spec_is_findable);
    return UNITY_END();
}
