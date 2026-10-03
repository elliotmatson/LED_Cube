#ifndef FIRMWARE_IMAGE_H
#define FIRMWARE_IMAGE_H

// Decides, from the first bytes of an upload, whether it is an application
// image this cube can boot. Hardware independent -- it reads the ESP image
// format by offset rather than through ESP-IDF's structs -- so every rejection
// is unit tested on the host (test/test_firmware_image).
//
// Layout (esp_app_format.h, esp_app_desc.h, ESP-IDF 5.5):
//   0    esp_image_header_t, 24 bytes: magic 0xE9 at 0, chip_id (LE16) at 12
//   24   first esp_image_segment_header_t, 8 bytes
//   32   esp_app_desc_t, 256 bytes: magic_word 0xABCD5432 at +0,
//        project_name[32] at +48
//   288  the cube's custom descriptor (.rodata_custom_desc): cookie[32]

#include <stddef.h>
#include <stdint.h>

namespace firmware_image
{
    constexpr size_t APP_DESC_OFFSET = 24 + 8;
    constexpr size_t PROJECT_NAME_OFFSET = APP_DESC_OFFSET + 48;
    constexpr size_t PROJECT_NAME_SIZE = 32;
    constexpr size_t COOKIE_OFFSET = APP_DESC_OFFSET + 256;
    constexpr size_t COOKIE_SIZE = 32;
    /// Bytes check() needs. Real uploads arrive in ~1.4 KB chunks.
    constexpr size_t CHECK_BYTES = COOKIE_OFFSET + COOKIE_SIZE;

    constexpr uint8_t IMAGE_MAGIC = 0xE9;
    constexpr uint32_t APP_DESC_MAGIC = 0xABCD5432;

    struct Expected
    {
        uint16_t chipId;         // CONFIG_IDF_FIRMWARE_CHIP_ID
        const char *projectName; // the running image's project_name
        const char *cookie;      // required signature, or nullptr if not required
    };

    /**
     * @param data The start of the upload.
     * @return nullptr if the image is acceptable, otherwise a reason fit to
     * show the user.
     */
    const char *check(const uint8_t *data, size_t len, const Expected &expected);

    /**
     * check() without the image header: the app descriptor's magic, the
     * project name and the cookie. For an image read back from flash while
     * Arduino's Update is still writing it -- Update holds back the first 16
     * bytes (magic and chip id included) until end(), so a partial image can
     * never boot, and they read as 0xFF until then. esp_ota_end() checks the
     * header itself when end() runs.
     */
    const char *checkDescriptor(const uint8_t *data, size_t len, const Expected &expected);
}

#endif
