#include <unity.h>
#include <string.h>

#include <firmware_image.h>

// The upload card writes nothing until check() accepts the first chunk, and an
// image it wrongly accepts is marked bootable. Each case here is one a user
// can produce from a release's assets or another project's build.

using namespace firmware_image;

static const uint16_t S3 = 9; // ESP_CHIP_ID_ESP32S3
static const Expected CUBE{S3, "LED_Cube", "LED_CUBE_FW"};
static uint8_t image[1400];

static void buildApp(uint16_t chip, const char *project, const char *cookie)
{
    memset(image, 0xFF, sizeof(image));
    image[0] = IMAGE_MAGIC;
    image[12] = chip & 0xFF;
    image[13] = chip >> 8;
    const uint32_t magic = APP_DESC_MAGIC;
    for (int i = 0; i < 4; i++)
    {
        image[APP_DESC_OFFSET + i] = (magic >> (8 * i)) & 0xFF;
    }
    memset(image + PROJECT_NAME_OFFSET, 0, PROJECT_NAME_SIZE);
    strncpy(reinterpret_cast<char *>(image + PROJECT_NAME_OFFSET), project, PROJECT_NAME_SIZE);
    memset(image + COOKIE_OFFSET, 0, COOKIE_SIZE);
    strncpy(reinterpret_cast<char *>(image + COOKIE_OFFSET), cookie, COOKIE_SIZE);
}

void setUp(void) { buildApp(S3, "LED_Cube", "LED_CUBE_FW"); }
void tearDown(void) {}

void test_a_cube_app_image_is_accepted(void)
{
    TEST_ASSERT_NULL(check(image, sizeof(image), CUBE));
}

void test_a_first_chunk_too_short_to_check_is_rejected(void)
{
    TEST_ASSERT_NOT_NULL(check(image, CHECK_BYTES - 1, CUBE));
    TEST_ASSERT_NULL(check(image, CHECK_BYTES, CUBE));
    TEST_ASSERT_NOT_NULL(check(nullptr, 0, CUBE));
}

void test_random_bytes_are_rejected(void)
{
    image[0] = 0x00;
    TEST_ASSERT_EQUAL_STRING("Not a firmware image", check(image, sizeof(image), CUBE));
}

void test_an_image_for_another_chip_is_rejected(void)
{
    buildApp(0 /* ESP32 */, "LED_Cube", "LED_CUBE_FW");
    TEST_ASSERT_EQUAL_STRING("Built for a different chip", check(image, sizeof(image), CUBE));
}

// The -factory asset starts with the bootloader: right magic, right chip, but
// no app descriptor. Written to the app slot it would never boot.
void test_the_factory_image_is_rejected(void)
{
    memset(image + APP_DESC_OFFSET, 0x00, 4);
    TEST_ASSERT_EQUAL_STRING("Not an application image (use the .bin without -factory)", check(image, sizeof(image), CUBE));
}

void test_another_projects_image_is_rejected(void)
{
    buildApp(S3, "hub", "LED_CUBE_FW");
    TEST_ASSERT_EQUAL_STRING("Not LED Cube firmware", check(image, sizeof(image), CUBE));
}

void test_a_project_name_that_only_starts_the_same_is_rejected(void)
{
    buildApp(S3, "LED_Cube_v2", "LED_CUBE_FW");
    TEST_ASSERT_NOT_NULL(check(image, sizeof(image), CUBE));
}

void test_a_missing_signature_is_rejected_only_when_required(void)
{
    buildApp(S3, "LED_Cube", "");
    TEST_ASSERT_EQUAL_STRING("Missing the cube firmware signature", check(image, sizeof(image), CUBE));
    Expected unsigned_ok = CUBE;
    unsigned_ok.cookie = nullptr;
    TEST_ASSERT_NULL(check(image, sizeof(image), unsigned_ok));
}

// ArduinoOTA and the GitHub updater check the image as written to flash,
// before Update.end() -- when the first 16 bytes are still 0xFF. The full
// check rejects that; the descriptor check must not.
void test_the_descriptor_check_ignores_the_held_back_header(void)
{
    memset(image, 0xFF, 16);
    TEST_ASSERT_EQUAL_STRING("Not a firmware image", check(image, sizeof(image), CUBE));
    TEST_ASSERT_NULL(checkDescriptor(image, sizeof(image), CUBE));
}

void test_the_descriptor_check_still_rejects_other_projects_and_unsigned_images(void)
{
    buildApp(S3, "hub", "LED_CUBE_FW");
    memset(image, 0xFF, 16);
    TEST_ASSERT_EQUAL_STRING("Not LED Cube firmware", checkDescriptor(image, sizeof(image), CUBE));
    buildApp(S3, "LED_Cube", "");
    memset(image, 0xFF, 16);
    TEST_ASSERT_EQUAL_STRING("Missing the cube firmware signature", checkDescriptor(image, sizeof(image), CUBE));
}


static int cmp(const char *a, const char *b) { return firmware_image::compareVersions(a, b); }

void test_versions_compare_by_number(void)
{
    TEST_ASSERT_TRUE(cmp("v0.10.0", "v0.9.0") > 0); // not as strings
    TEST_ASSERT_TRUE(cmp("v0.6.0", "v0.5.9") > 0);
    TEST_ASSERT_TRUE(cmp("v1", "v0.99.99") > 0);
    TEST_ASSERT_EQUAL_INT(0, cmp("v0.6.0", "0.6.0"));
    TEST_ASSERT_EQUAL_INT(0, cmp("v0.6", "v0.6.0"));
}

void test_pre_releases_come_before_the_release(void)
{
    TEST_ASSERT_TRUE(cmp("v0.6.0-b1", "v0.6.0") < 0);
    TEST_ASSERT_TRUE(cmp("v0.6.0-b2", "v0.6.0-b1") > 0);
    TEST_ASSERT_TRUE(cmp("v0.6.0-b10", "v0.6.0-b2") > 0); // b10 after b2
    TEST_ASSERT_TRUE(cmp("v0.6.0-b1", "v0.5.9") > 0);
    TEST_ASSERT_TRUE(cmp("v0.6.0-rc1", "v0.6.0-b9") > 0); // letters, then number
    TEST_ASSERT_EQUAL_INT(0, cmp("v0.6.0+abc", "v0.6.0"));
}

void test_only_newer_versions_are_updates(void)
{
    TEST_ASSERT_TRUE(firmware_image::isNewer("v0.6.0", "v0.6.0-b1"));
    TEST_ASSERT_FALSE(firmware_image::isNewer("v0.6.0-b1", "v0.6.0-b1"));
    // The case that could have bitten: the stable channel's "latest" is an
    // old release, and it must never be installed over a newer beta.
    TEST_ASSERT_FALSE(firmware_image::isNewer("v0.4.10", "v0.6.0-b1"));
}

void test_non_versions_never_update(void)
{
    bool ok = true;
    TEST_ASSERT_EQUAL_INT(0, firmware_image::compareVersions("v0.6.0", "DEV", &ok));
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_FALSE(firmware_image::isNewer("v9.9.9", "DEV"));
    TEST_ASSERT_FALSE(firmware_image::isNewer("latest", "v0.6.0"));
    TEST_ASSERT_FALSE(firmware_image::isNewer("v0.6.0-", "v0.5.0"));
    TEST_ASSERT_FALSE(firmware_image::isNewer(nullptr, "v0.5.0"));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_cube_app_image_is_accepted);
    RUN_TEST(test_a_first_chunk_too_short_to_check_is_rejected);
    RUN_TEST(test_random_bytes_are_rejected);
    RUN_TEST(test_an_image_for_another_chip_is_rejected);
    RUN_TEST(test_the_factory_image_is_rejected);
    RUN_TEST(test_another_projects_image_is_rejected);
    RUN_TEST(test_a_project_name_that_only_starts_the_same_is_rejected);
    RUN_TEST(test_a_missing_signature_is_rejected_only_when_required);
    RUN_TEST(test_the_descriptor_check_ignores_the_held_back_header);
    RUN_TEST(test_the_descriptor_check_still_rejects_other_projects_and_unsigned_images);
    RUN_TEST(test_versions_compare_by_number);
    RUN_TEST(test_pre_releases_come_before_the_release);
    RUN_TEST(test_only_newer_versions_are_updates);
    RUN_TEST(test_non_versions_never_update);
    return UNITY_END();
}
