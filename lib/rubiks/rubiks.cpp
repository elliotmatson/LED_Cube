#include "rubiks.h"

namespace rubiks
{
    void Cube::reset()
    {
        int n = 0;
        for (uint8_t axis = 0; axis < 3; axis++)
        {
            for (int8_t side = -1; side <= 1; side += 2)
            {
                const Color c = Color(axis == 2 ? (side > 0 ? White : Yellow)
                                                : axis == 0 ? (side > 0 ? Red : Orange)
                                                            : (side > 0 ? Green : Blue));
                for (int8_t a = -1; a <= 1; a++)
                {
                    for (int8_t b = -1; b <= 1; b++)
                    {
                        Sticker &s = _stickers[n++];
                        const uint8_t u = (axis + 1) % 3, v = (axis + 2) % 3;
                        s.pos[axis] = side;
                        s.pos[u] = a;
                        s.pos[v] = b;
                        s.normal[0] = s.normal[1] = s.normal[2] = 0;
                        s.normal[axis] = side;
                        s.color = c;
                    }
                }
            }
        }
    }

    // A quarter turn about `axis`, `dir` = +1 counter-clockwise looking from
    // the positive end of the axis.
    static void rotate(int8_t v[3], uint8_t axis, int8_t dir)
    {
        const uint8_t u = (axis + 1) % 3, w = (axis + 2) % 3;
        const int8_t a = v[u], b = v[w];
        if (dir > 0)
        {
            v[u] = int8_t(-b);
            v[w] = a;
        }
        else
        {
            v[u] = b;
            v[w] = int8_t(-a);
        }
    }

    void Cube::apply(const Move &move)
    {
        for (Sticker &s : _stickers)
        {
            if (s.pos[move.axis] == move.layer)
            {
                rotate(s.pos, move.axis, move.dir);
                rotate(s.normal, move.axis, move.dir);
            }
        }
    }

    bool Cube::solved() const
    {
        // Every sticker facing the same way has the same colour.
        for (const Sticker &a : _stickers)
        {
            for (const Sticker &b : _stickers)
            {
                if (a.normal[0] == b.normal[0] && a.normal[1] == b.normal[1] && a.normal[2] == b.normal[2] && a.color != b.color)
                {
                    return false;
                }
            }
        }
        return true;
    }

    Color Cube::at(const int8_t pos[3], const int8_t normal[3]) const
    {
        for (const Sticker &s : _stickers)
        {
            if (s.pos[0] == pos[0] && s.pos[1] == pos[1] && s.pos[2] == pos[2] &&
                s.normal[0] == normal[0] && s.normal[1] == normal[1] && s.normal[2] == normal[2])
            {
                return s.color;
            }
        }
        return White; // unreachable for a valid position and normal
    }

    Move randomMove(const Move *previous, uint32_t (*rnd)())
    {
        for (;;)
        {
            Move m{uint8_t(rnd() % 3), int8_t(rnd() % 2 ? 1 : -1), int8_t(rnd() % 2 ? 1 : -1)};
            if (previous == nullptr || m.axis != previous->axis || m.layer != previous->layer)
            {
                return m;
            }
        }
    }
}
