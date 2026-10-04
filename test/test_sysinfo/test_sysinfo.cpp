#include <unity.h>
#include <string.h>

#include <sysinfo.h>

void setUp(void) {}
void tearDown(void) {}

void test_names_reset_reasons(void)
{
    TEST_ASSERT_EQUAL_STRING("power-on", sysinfo::resetReasonName(1));
    TEST_ASSERT_EQUAL_STRING("panic", sysinfo::resetReasonName(4));
    TEST_ASSERT_EQUAL_STRING("task watchdog", sysinfo::resetReasonName(6));
    TEST_ASSERT_EQUAL_STRING("cpu lockup", sysinfo::resetReasonName(15));
    TEST_ASSERT_EQUAL_STRING("unknown", sysinfo::resetReasonName(-1));
    TEST_ASSERT_EQUAL_STRING("unknown", sysinfo::resetReasonName(16));
}

void test_formats_uptime_as_the_two_largest_units(void)
{
    char out[24];
    sysinfo::formatUptime(0, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("0s", out);
    sysinfo::formatUptime(59, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("59s", out);
    sysinfo::formatUptime(12 * 60 + 5, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("12m 5s", out);
    sysinfo::formatUptime(4 * 3600 + 12 * 60 + 5, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("4h 12m", out);
    sysinfo::formatUptime(3 * 86400 + 4 * 3600 + 59, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("3d 4h", out);
    sysinfo::formatUptime(0xFFFFFFFF, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("49710d 6h", out);
}

void test_formats_bytes(void)
{
    char out[16];
    sysinfo::formatBytes(512, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("512 B", out);
    sysinfo::formatBytes(1024, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("1.0 KB", out);
    sysinfo::formatBytes(126362, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("123.4 KB", out);
    sysinfo::formatBytes(8192000, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("7.81 MB", out);
}

void test_parses_ipv4_and_ipv6_answers(void)
{
    char out[46];
    TEST_ASSERT_TRUE(sysinfo::parseIp("203.0.113.7", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("203.0.113.7", out);
    TEST_ASSERT_TRUE(sysinfo::parseIp("  198.51.100.255\r\n", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("198.51.100.255", out);
    TEST_ASSERT_TRUE(sysinfo::parseIp("2001:db8::1\n", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("2001:db8::1", out);
    TEST_ASSERT_TRUE(sysinfo::parseIp("::ffff:192.0.2.128", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("::ffff:192.0.2.128", out);
}

void test_rejects_anything_but_a_bare_address(void)
{
    char out[46] = "unchanged";
    TEST_ASSERT_FALSE(sysinfo::parseIp(nullptr, out, sizeof(out)));
    TEST_ASSERT_FALSE(sysinfo::parseIp("", out, sizeof(out)));
    TEST_ASSERT_FALSE(sysinfo::parseIp("<html><body>Log in</body></html>", out, sizeof(out)));
    TEST_ASSERT_FALSE(sysinfo::parseIp("256.1.1.1", out, sizeof(out)));
    TEST_ASSERT_FALSE(sysinfo::parseIp("1.2.3", out, sizeof(out)));
    TEST_ASSERT_FALSE(sysinfo::parseIp("1.2.3.4.5", out, sizeof(out)));
    TEST_ASSERT_FALSE(sysinfo::parseIp("1.2.3.4:80", out, sizeof(out)));
    TEST_ASSERT_FALSE(sysinfo::parseIp("1.2.3.a", out, sizeof(out)));
    TEST_ASSERT_FALSE(sysinfo::parseIp("1.2.3 4", out, sizeof(out)));
    TEST_ASSERT_FALSE(sysinfo::parseIp("deadbeef", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("unchanged", out);
}

void test_rejects_an_address_that_does_not_fit(void)
{
    char out[8] = "x";
    TEST_ASSERT_FALSE(sysinfo::parseIp("203.0.113.7", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("x", out);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_names_reset_reasons);
    RUN_TEST(test_formats_uptime_as_the_two_largest_units);
    RUN_TEST(test_formats_bytes);
    RUN_TEST(test_parses_ipv4_and_ipv6_answers);
    RUN_TEST(test_rejects_anything_but_a_bare_address);
    RUN_TEST(test_rejects_an_address_that_does_not_fit);
    return UNITY_END();
}
