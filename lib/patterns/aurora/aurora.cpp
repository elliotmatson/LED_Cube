#include "aurora.h"

#include <math.h>

namespace
{
    float SIN[256];   // one cycle
    float GLOW[64];   // a ribbon's brightness by distance, quarter pixels

    struct Ribbon
    {
        float centre0;  // where it starts across the diagonal (u + v)
        float drift;    // how fast it moves across, per second
        float amp;      // wiggle size
        float freq;     // wiggle frequency along it (table steps per pixel)
        float wiggle;   // wiggle speed, table steps per second
        float bright;
    };
    // Spread across the top; the outer two reach the side faces.
    const Ribbon RIBBONS_DEF[Aurora::RIBBONS] = {
        {40.0f, 2.2f, 7.0f, 2.3f, 21.0f, 1.00f},
        {78.0f, 1.6f, 9.0f, 1.7f, -15.0f, 0.85f},
        {108.0f, 2.8f, 6.0f, 2.9f, 27.0f, 0.75f},
    };
    // The span ribbons drift across before wrapping round.
    const float W_MIN = 10.0f, W_MAX = 150.0f;

    // Curtains on the side strip: lower edge row, its wave, and how far up
    // the light reaches before it is all violet.
    struct Curtain
    {
        float edge;   // strip row of the lower edge (0 = top)
        float amp;    // main wave
        float freq;   // table steps per pixel
        float wave;   // table steps per second
        float amp2;   // a faster, smaller wave on top
        float height; // pixels from green to violet
        float bright;
    };
    const Curtain CURTAINS_DEF[] = {
        {38.0f, 9.0f, 3.1f, 19.0f, 3.0f, 26.0f, 1.0f},
        {26.0f, 7.0f, 2.2f, -13.0f, 2.5f, 20.0f, 0.55f},
    };
    float FADE[256]; // brightness by height above the edge, quarter pixels

    const color::RGB AURORA_GREEN = {30, 255, 110};
    const color::RGB AURORA_VIOLET = {150, 40, 255};

    inline float sinAt(float x) { return SIN[uint8_t(int32_t(x))]; }
}

Aurora::Aurora()
{
    data.id = "aurora";
    data.name = "Aurora";
}

Aurora::~Aurora()
{
    end();
}

void Aurora::begin(PatternServices *services)
{
    pattern = services;
    for (int i = 0; i < 256; i++)
    {
        SIN[i] = sinf(i * 2 * float(M_PI) / 256);
    }
    for (int i = 0; i < 256; i++)
    {
        // Bright just above the edge, fading over ~25 px.
        FADE[i] = expf(-(i / 4.0f) / 14.0f);
    }
    for (int i = 0; i < 64; i++)
    {
        const float d = i / 4.0f; // pixels
        GLOW[i] = expf(-(d * d) / 9.0f); // half-width about 2.5 px
    }
    places = static_cast<Place *>(heap_caps_malloc(cube::FACE_SIZE * cube::FACE_SIZE * sizeof(Place), MALLOC_CAP_SPIRAM));
    if (places == nullptr)
    {
        return;
    }
    for (int16_t y = 0; y < cube::CHAIN_HEIGHT; y++)
    {
        // Face 0, chain x 0-63, is the top: the z = 64 plane.
        for (int16_t x = 0; x < cube::FACE_SIZE; x++)
        {
            const cube::Vec3 p = cube::toCube({x, y});
            Place &pl = places[y * cube::FACE_SIZE + x];
            pl.s = int8_t(lroundf(p.x - p.y));
            pl.w = uint8_t(lroundf(p.x + p.y));
        }
    }
    // Stars on the top face, away from the edges.
    for (int i = 0; i < STARS; i++)
    {
        const int16_t sx = int16_t(4 + esp_random() % 56), sy = int16_t(4 + esp_random() % 56);
        stars[i] = uint16_t(sy * cube::CHAIN_WIDTH + sx); // face 0 is chain x 0-63
    }
    startMs = millis();
}

void Aurora::end()
{
    free(places);
    places = nullptr;
}

void Aurora::tick()
{
    if (places == nullptr)
    {
        return;
    }
    const float t = (millis() - startMs) / 1000.0f;
    // The whole display breathes a little.
    const float breathe = 0.85f + 0.15f * sinAt(t * 11.0f);

    // The sides, as seen from the ground: curtains running along the strip
    // (BottomPanels: strip x 0-127, y 0 at the top), each with a sharp,
    // bright green lower edge, fading to violet as it rises, textured with
    // vertical rays. Edge and rays depend only on the column.
    const int STRIP_W = 2 * cube::FACE_SIZE;
    float edge[CURTAINS][STRIP_W], rays[CURTAINS][STRIP_W];
    for (int k = 0; k < CURTAINS; k++)
    {
        const Curtain &c = CURTAINS_DEF[k];
        for (int x = 0; x < STRIP_W; x++)
        {
            edge[k][x] = c.edge + c.amp * sinAt(x * c.freq + t * c.wave) + c.amp2 * sinAt(x * c.freq * 2.7f - t * c.wave * 1.3f);
            const float r = 0.55f + 0.3f * sinAt(x * 13.0f + t * 17.0f * (k + 1)) + 0.15f * sinAt(x * 5.0f - t * 7.0f);
            rays[k][x] = r * c.bright * breathe;
        }
    }

    // The top face, as seen from below: ribbons across it.
    float centre[RIBBONS], phase[RIBBONS];
    for (int k = 0; k < RIBBONS; k++)
    {
        const Ribbon &r = RIBBONS_DEF[k];
        centre[k] = W_MIN + fmodf(r.centre0 - W_MIN + r.drift * t, W_MAX - W_MIN);
        phase[k] = r.wiggle * t;
    }

    const Place *pl = places;
    for (int16_t row = 0; row < cube::CHAIN_HEIGHT; row++)
    {
        uint8_t *out = pattern->display->rowForWrite(row, 0, cube::CHAIN_WIDTH);
        // Top face: chain x 0-63.
        for (int16_t col = 0; col < cube::FACE_SIZE; col++, pl++)
        {
            const float s = pl->s, w = pl->w;
            float glow = 0;
            for (int k = 0; k < RIBBONS; k++)
            {
                const Ribbon &r = RIBBONS_DEF[k];
                const float line = centre[k] + r.amp * sinAt(s * r.freq + phase[k]);
                const int d = int(fabsf(w - line) * 4);
                if (d < 64)
                {
                    glow += GLOW[d] * r.bright;
                }
            }
            glow *= breathe * (0.75f + 0.25f * sinAt(s * 9.0f + t * 13.0f));
            const float level = glow > 1 ? 1 : glow;
            const color::RGB c = color::lerp(AURORA_VIOLET, AURORA_GREEN, uint8_t(level * 255));
            *out++ = uint8_t(c.r * level);
            *out++ = uint8_t(c.g * level);
            *out++ = uint8_t(c.b * level);
        }
        // The side strip: chain x 64-191 is strip x 127-0, strip y 63 - row.
        const int sy = cube::CHAIN_HEIGHT - 1 - row;
        for (int16_t col = cube::FACE_SIZE; col < cube::CHAIN_WIDTH; col++)
        {
            const int sx = cube::CHAIN_WIDTH - 1 - col;
            float r = 0, g = 0, b = 0;
            for (int k = 0; k < CURTAINS; k++)
            {
                // Rows above the edge: d > 0. A two-pixel soft lower edge,
                // then light fading upwards, green turning violet.
                const float d = edge[k][sx] - sy;
                if (d < -2)
                {
                    continue;
                }
                const float up = d < 0 ? 0 : d;
                const float fall = d < 0 ? (d + 2) / 2 : FADE[min(int(up * 4), 255)];
                const float level = fall * rays[k][sx];
                const float violet = min(1.0f, up / CURTAINS_DEF[k].height);
                r += level * (AURORA_GREEN.r + (AURORA_VIOLET.r - AURORA_GREEN.r) * violet);
                g += level * (AURORA_GREEN.g + (AURORA_VIOLET.g - AURORA_GREEN.g) * violet);
                b += level * (AURORA_GREEN.b + (AURORA_VIOLET.b - AURORA_GREEN.b) * violet);
            }
            *out++ = uint8_t(min(r, 255.0f));
            *out++ = uint8_t(min(g, 255.0f));
            *out++ = uint8_t(min(b, 255.0f));
        }
    }

    // Stars, dim and twinkling, where the sky is dark.
    for (int i = 0; i < STARS; i++)
    {
        const int16_t x = int16_t(stars[i] % cube::CHAIN_WIDTH), y = int16_t(stars[i] / cube::CHAIN_WIDTH);
        const float tw = 0.5f + 0.5f * sinAt(t * (20.0f + i) + i * 37.0f);
        const uint8_t v = uint8_t(25 + 45 * tw);
        uint8_t *p = pattern->display->rowForWrite(y, x, 1);
        if (p && p[0] + p[1] + p[2] < v)
        {
            p[0] = p[1] = p[2] = v;
        }
    }
}
