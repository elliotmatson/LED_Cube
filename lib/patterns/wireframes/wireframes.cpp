#include "wireframes.h"

#include <math.h>

namespace
{
    struct Solid
    {
        const float (*vertices)[3];
        int vertexCount;
        const uint8_t (*edges)[2];
        int edgeCount;
    };

    const float CUBE_V[][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
    const uint8_t CUBE_E[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

    const float OCTA_V[][3] = {{1.4f, 0, 0}, {-1.4f, 0, 0}, {0, 1.4f, 0}, {0, -1.4f, 0}, {0, 0, 1.4f}, {0, 0, -1.4f}};
    const uint8_t OCTA_E[][2] = {{0, 2}, {0, 3}, {0, 4}, {0, 5}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {2, 4}, {2, 5}, {3, 4}, {3, 5}};

    // Icosahedron: (0, ±1, ±phi) and cyclic permutations, scaled to radius ~1.4.
    const float P = 1.618034f * 0.74f, O = 0.74f;
    const float ICO_V[][3] = {{0, O, P}, {0, -O, P}, {0, O, -P}, {0, -O, -P}, {O, P, 0}, {-O, P, 0},
                              {O, -P, 0}, {-O, -P, 0}, {P, 0, O}, {-P, 0, O}, {P, 0, -O}, {-P, 0, -O}};
    const uint8_t ICO_E[][2] = {{0, 1}, {0, 4}, {0, 5}, {0, 8}, {0, 9}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {2, 3},
                                {2, 4}, {2, 5}, {2, 10}, {2, 11}, {3, 6}, {3, 7}, {3, 10}, {3, 11}, {4, 5}, {4, 8},
                                {4, 10}, {5, 9}, {5, 11}, {6, 7}, {6, 8}, {6, 10}, {7, 9}, {7, 11}, {8, 10}, {9, 11}};

    const Solid SOLIDS[] = {
        {CUBE_V, 8, CUBE_E, 12},
        {OCTA_V, 6, OCTA_E, 12},
        {ICO_V, 12, ICO_E, 30},
    };
    const int SOLID_COUNT = sizeof(SOLIDS) / sizeof(SOLIDS[0]);
    const uint32_t SOLID_MS = 12000;   // each solid's turn
    const float RADIUS = 26.0f;        // projected units per model unit
    const float CENTRE_X = 0.0f, CENTRE_Y = 0.75f; // the shared corner, projected
}

Wireframes::Wireframes()
{
    data.id = "wireframes";
    data.name = "Wireframes";
}

void Wireframes::begin(PatternServices *services)
{
    pattern = services;
    startMs = millis();
}

void Wireframes::drawEdge(const float *a, const float *b, uint8_t hue)
{
    const float dx = b[0] - a[0], dy = b[1] - a[1];
    const int steps = int(sqrtf(dx * dx + dy * dy) * 2) + 1; // half-unit steps
    for (int i = 0; i <= steps; i++)
    {
        const float f = float(i) / steps;
        cube::Point p = cube::unproject(a[0] + dx * f, a[1] + dy * f);
        if (!p.valid())
        {
            continue; // off the hexagon, or in a seam gap
        }
        // Depth: z in [-1.4, 1.4] model units; nearer is brighter.
        const float z = a[2] + (b[2] - a[2]) * f;
        const uint8_t v = uint8_t(fminf(255.0f, 150 + z * 70));
        color::RGB c = color::hsv(hue, 200, v);
        pattern->display->drawPixelRGB888(p.x, p.y, c.r, c.g, c.b);
    }
}

void Wireframes::tick()
{
    const uint32_t ms = millis() - startMs;
    const Solid &solid = SOLIDS[(ms / SOLID_MS) % SOLID_COUNT];
    const float t = ms / 1000.0f;
    const float ax = t * 0.7f, ay = t * 0.45f, az = t * 0.2f;
    const float cx = cosf(ax), sx = sinf(ax), cy = cosf(ay), sy = sinf(ay), cz = cosf(az), sz = sinf(az);
    const uint8_t hue = uint8_t(ms / 60);

    // Rotate, then project orthographically onto the plane: (x, y) in the
    // plane, z towards the viewer.
    float projected[12][3];
    for (int i = 0; i < solid.vertexCount; i++)
    {
        float x = solid.vertices[i][0], y = solid.vertices[i][1], z = solid.vertices[i][2];
        float y1 = y * cx - z * sx, z1 = y * sx + z * cx;
        float x2 = x * cy + z1 * sy, z2 = -x * sy + z1 * cy;
        float x3 = x2 * cz - y1 * sz, y3 = x2 * sz + y1 * cz;
        projected[i][0] = CENTRE_X + x3 * RADIUS;
        projected[i][1] = CENTRE_Y + y3 * RADIUS;
        projected[i][2] = z2;
    }

    pattern->display->fillScreen(0);
    for (int e = 0; e < solid.edgeCount; e++)
    {
        drawEdge(projected[solid.edges[e][0]], projected[solid.edges[e][1]], uint8_t(hue + e * 4));
    }
}
