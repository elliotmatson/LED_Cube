#ifndef COLOR_H
#define COLOR_H

// Small colour helpers for patterns. Header-only and hardware independent
// (test/test_color).

#include <stdint.h>

namespace color
{
    struct RGB
    {
        uint8_t r, g, b;
    };

    /// Hue 0-255 around the colour wheel (0 red, 85 green, 170 blue),
    /// saturation and value 0-255. Integer only.
    inline RGB hsv(uint8_t h, uint8_t s, uint8_t v)
    {
        if (s == 0)
        {
            return RGB{v, v, v};
        }
        const uint8_t region = h / 43;
        const uint8_t remainder = (h - region * 43) * 6;
        const uint8_t p = (v * (255 - s)) >> 8;
        const uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
        const uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
        switch (region)
        {
        case 0:
            return RGB{v, t, p};
        case 1:
            return RGB{q, v, p};
        case 2:
            return RGB{p, v, t};
        case 3:
            return RGB{p, q, v};
        case 4:
            return RGB{t, p, v};
        default:
            return RGB{v, p, q};
        }
    }

    /// Linear blend: t = 0 gives a, 255 gives b.
    inline RGB lerp(RGB a, RGB b, uint8_t t)
    {
        return RGB{uint8_t(a.r + (((int(b.r) - a.r) * t) / 255)),
                   uint8_t(a.g + (((int(b.g) - a.g) * t) / 255)),
                   uint8_t(a.b + (((int(b.b) - a.b) * t) / 255))};
    }

    /// A gradient through `count` evenly spaced stops; t = 0 is the first
    /// stop, 255 the last.
    inline RGB gradient(const RGB *stops, int count, uint8_t t)
    {
        if (count <= 1)
        {
            return count == 1 ? stops[0] : RGB{0, 0, 0};
        }
        const int scaled = t * (count - 1);
        const int i = scaled / 255;
        if (i >= count - 1)
        {
            return stops[count - 1];
        }
        return lerp(stops[i], stops[i + 1], uint8_t((scaled % 255)));
    }

    /// Scales a colour by v / 255.
    inline RGB scale(RGB c, uint8_t v)
    {
        return RGB{uint8_t((c.r * v) / 255), uint8_t((c.g * v) / 255), uint8_t((c.b * v) / 255)};
    }
}

#endif
