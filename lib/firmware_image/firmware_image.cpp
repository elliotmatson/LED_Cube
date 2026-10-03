#include "firmware_image.h"

#include <string.h>

namespace firmware_image
{
    static uint16_t le16(const uint8_t *p) { return uint16_t(p[0] | (p[1] << 8)); }
    static uint32_t le32(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }

    const char *check(const uint8_t *data, size_t len, const Expected &expected)
    {
        if (data == nullptr || len < CHECK_BYTES)
        {
            return "Too short to be a firmware image";
        }
        // Every ESP image starts with this -- including the bootloader at the
        // front of a -factory image -- which is all Arduino's Update checks.
        if (data[0] != IMAGE_MAGIC)
        {
            return "Not a firmware image";
        }
        if (le16(data + 12) != expected.chipId)
        {
            return "Built for a different chip";
        }
        return checkDescriptor(data, len, expected);
    }

    const char *checkDescriptor(const uint8_t *data, size_t len, const Expected &expected)
    {
        if (data == nullptr || len < CHECK_BYTES)
        {
            return "Too short to be a firmware image";
        }
        if (le32(data + APP_DESC_OFFSET) != APP_DESC_MAGIC)
        {
            // A -factory image starts with the bootloader, which has no app
            // descriptor here.
            return "Not an application image (use the .bin without -factory)";
        }
        if (strncmp(reinterpret_cast<const char *>(data + PROJECT_NAME_OFFSET), expected.projectName, PROJECT_NAME_SIZE) != 0)
        {
            return "Not LED Cube firmware";
        }
        if (expected.cookie && strncmp(reinterpret_cast<const char *>(data + COOKIE_OFFSET), expected.cookie, COOKIE_SIZE) != 0)
        {
            return "Missing the cube firmware signature";
        }
        return nullptr;
    }
}
