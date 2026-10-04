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

namespace
{
    struct Version
    {
        unsigned long part[3] = {0, 0, 0};
        const char *pre = nullptr; // pre-release, or nullptr for a release
        size_t preLen = 0;
    };

    bool isDigit(char c) { return c >= '0' && c <= '9'; }

    /// Parses [v]N[.N[.N]][-pre][+build]. False if it is not that.
    bool parse(const char *s, Version &v)
    {
        if (s == nullptr)
        {
            return false;
        }
        if (*s == 'v' || *s == 'V')
        {
            s++;
        }
        for (int i = 0; i < 3; i++)
        {
            if (!isDigit(*s))
            {
                return false;
            }
            unsigned long n = 0;
            while (isDigit(*s))
            {
                n = n * 10 + unsigned(*s++ - '0');
            }
            v.part[i] = n;
            if (*s != '.')
            {
                break;
            }
            s++;
        }
        if (*s == '-')
        {
            v.pre = ++s;
            while (*s && *s != '+')
            {
                s++;
            }
            v.preLen = size_t(s - v.pre);
            if (v.preLen == 0)
            {
                return false;
            }
        }
        return *s == '\0' || *s == '+';
    }

    /// Compares pre-release strings, numbers as numbers: "b2" < "b10".
    int comparePre(const char *a, size_t na, const char *b, size_t nb)
    {
        size_t i = 0, j = 0;
        while (i < na && j < nb)
        {
            if (isDigit(a[i]) && isDigit(b[j]))
            {
                unsigned long x = 0, y = 0;
                while (i < na && isDigit(a[i]))
                {
                    x = x * 10 + unsigned(a[i++] - '0');
                }
                while (j < nb && isDigit(b[j]))
                {
                    y = y * 10 + unsigned(b[j++] - '0');
                }
                if (x != y)
                {
                    return x < y ? -1 : 1;
                }
            }
            else
            {
                if (a[i] != b[j])
                {
                    return (unsigned char)a[i] < (unsigned char)b[j] ? -1 : 1;
                }
                i++;
                j++;
            }
        }
        // A longer pre-release with the same start is later ("b1" < "b1.1").
        return (i < na) - (j < nb);
    }
}

int firmware_image::compareVersions(const char *a, const char *b, bool *ok)
{
    Version x, y;
    const bool valid = parse(a, x) && parse(b, y);
    if (ok)
    {
        *ok = valid;
    }
    if (!valid)
    {
        return 0;
    }
    for (int i = 0; i < 3; i++)
    {
        if (x.part[i] != y.part[i])
        {
            return x.part[i] < y.part[i] ? -1 : 1;
        }
    }
    if (!x.pre || !y.pre)
    {
        // A release is newer than any of its pre-releases.
        return (x.pre ? -1 : 0) + (y.pre ? 1 : 0);
    }
    return comparePre(x.pre, x.preLen, y.pre, y.preLen);
}

bool firmware_image::isNewer(const char *candidate, const char *running)
{
    bool ok = false;
    const int c = compareVersions(candidate, running, &ok);
    return ok && c > 0;
}
