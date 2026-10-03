#include "cube_geometry.h"

namespace cube
{
    Point faceToChain(int faceIndex, int rotation, int16_t x, int16_t y)
    {
        if (faceIndex < 0 || faceIndex >= FACES)
        {
            return NO_POINT;
        }
        int16_t fx = x;
        int16_t fy = y;
        switch (rotation & 3)
        {
        case 1: // 90 degrees
            fx = y;
            fy = FACE_SIZE - 1 - x;
            break;
        case 2: // 180 degrees
            fx = FACE_SIZE - 1 - x;
            fy = FACE_SIZE - 1 - y;
            break;
        case 3: // 270 degrees
            fx = FACE_SIZE - 1 - y;
            fy = x;
            break;
        }
        if (fx < 0 || fx >= FACE_SIZE || fy < 0 || fy >= FACE_SIZE)
        {
            return NO_POINT;
        }
        return Point{static_cast<int16_t>(fx + faceIndex * FACE_SIZE), fy};
    }

    Step step(Point from, Dir dir)
    {
        const Step none{NO_POINT, dir};
        if (!from.valid() || from.x >= CHAIN_WIDTH || from.y < 0 || from.y >= CHAIN_HEIGHT)
        {
            return none;
        }
        const int16_t x = from.x;
        const int16_t y = from.y;
        const int16_t last = FACE_SIZE - 1;

        switch (face(x))
        {
        case 0:
            switch (dir)
            {
            case UP:
                return y == 0 ? none : Step{{x, int16_t(y - 1)}, UP};
            case RIGHT:
                // Onto face 1's bottom row, heading away from the seam.
                return x == last ? Step{{int16_t(FACE_SIZE + y), last}, UP} : Step{{int16_t(x + 1), y}, RIGHT};
            case DOWN:
                // Onto face 2's bottom row, mirrored, heading away from the seam.
                return y == last ? Step{{int16_t(CHAIN_WIDTH - 1 - x), last}, UP} : Step{{x, int16_t(y + 1)}, DOWN};
            case LEFT:
                return x == 0 ? none : Step{{int16_t(x - 1), y}, LEFT};
            }
            break;
        case 1:
            switch (dir)
            {
            case UP:
                return y == 0 ? none : Step{{x, int16_t(y - 1)}, UP};
            case RIGHT:
                // x = 127 -> 128 is the seam with face 2; same orientation.
                return Step{{int16_t(x + 1), y}, RIGHT};
            case DOWN:
                // Onto face 0's right edge, heading away from it.
                return y == last ? Step{{last, int16_t(x - FACE_SIZE)}, LEFT} : Step{{x, int16_t(y + 1)}, DOWN};
            case LEFT:
                return x == FACE_SIZE ? none : Step{{int16_t(x - 1), y}, LEFT};
            }
            break;
        case 2:
            switch (dir)
            {
            case UP:
                return y == 0 ? none : Step{{x, int16_t(y - 1)}, UP};
            case RIGHT:
                return x == CHAIN_WIDTH - 1 ? none : Step{{int16_t(x + 1), y}, RIGHT};
            case DOWN:
                // Onto face 0's bottom row, mirrored, heading away from it.
                return y == last ? Step{{int16_t(CHAIN_WIDTH - 1 - x), last}, UP} : Step{{x, int16_t(y + 1)}, DOWN};
            case LEFT:
                // x = 128 -> 127 is the seam with face 1.
                return Step{{int16_t(x - 1), y}, LEFT};
            }
            break;
        }
        return none;
    }

    void buildNeighbours(int16_t *out)
    {
        for (int16_t y = 0; y < CHAIN_HEIGHT; y++)
        {
            for (int16_t x = 0; x < CHAIN_WIDTH; x++)
            {
                const Point p{x, y};
                int16_t *n = out + cellIndex(p) * NEIGHBOURS;
                int count = 0;
                for (uint8_t d = 0; d < 4; d++)
                {
                    Step orth = step(p, Dir(d));
                    int16_t candidates[2] = {-1, -1};
                    if (orth.valid())
                    {
                        candidates[0] = int16_t(cellIndex(orth.to));
                        Step diag = step(orth.to, Dir((orth.dir + 1) & 3));
                        if (diag.valid())
                        {
                            candidates[1] = int16_t(cellIndex(diag.to));
                        }
                    }
                    for (int16_t c : candidates)
                    {
                        // Drop duplicates and the cell itself (both happen
                        // only around the shared corner).
                        bool keep = c >= 0 && c != cellIndex(p);
                        for (int k = 0; keep && k < count; k++)
                        {
                            keep = n[k] != c;
                        }
                        n[count++] = keep ? c : -1;
                    }
                }
            }
        }
    }

    Vec3 toCube(Point p)
    {
        const float px = p.x + 0.5f;
        const float py = p.y + 0.5f;
        switch (face(p.x))
        {
        case 0:
            return Vec3{px, py, float(FACE_SIZE)};
        case 1:
            return Vec3{float(FACE_SIZE), px - FACE_SIZE, py};
        default:
            return Vec3{float(CHAIN_WIDTH) - px, float(FACE_SIZE), py};
        }
    }

    // These reproduce the PROJ_CALC_* macros the patterns were written
    // against, so existing visuals do not change. The right shifts of negative
    // values are arithmetic on every compiler this builds with.
    int16_t projectX(int16_t x, int16_t y)
    {
        if (x < FACE_SIZE)
        {
            return (7 * (x - y)) >> 3;
        }
        if (x < 2 * FACE_SIZE)
        {
            return 112 - ((7 * x) >> 3);
        }
        return 109 - ((7 * x) >> 3);
    }

    float projectXf(int16_t x, int16_t y)
    {
        if (x < FACE_SIZE)
        {
            return 0.8660254f * (x - y);
        }
        if (x < 2 * FACE_SIZE)
        {
            return 111.7173f - 0.8660254f * x;
        }
        return 109.1192f - 0.8660254f * x;
    }

    int16_t projectY(int16_t x, int16_t y)
    {
        if (x < FACE_SIZE)
        {
            return (130 - x - y) >> 1;
        }
        if (x < 2 * FACE_SIZE)
        {
            return y - (x >> 1);
        }
        return (x >> 1) + y - 128;
    }
}
