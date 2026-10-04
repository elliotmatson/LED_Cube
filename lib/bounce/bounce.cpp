#include "bounce.h"

namespace
{
    /// Folds `pos` back into [0, max], flipping `v` once per wall crossed.
    bool reflect(int32_t &pos, int32_t &v, int32_t max)
    {
        bool hit = false;
        // A long step (or a tiny room) can cross more than one wall.
        while (pos < 0 || pos > max)
        {
            pos = pos < 0 ? -pos : 2 * max - pos;
            v = -v;
            hit = true;
            if (max <= 0)
            {
                pos = 0;
                break;
            }
        }
        return hit;
    }
}

bounce::Hits bounce::step(Body &body, int32_t maxX, int32_t maxY, uint32_t dtMs)
{
    body.x += int32_t(int64_t(body.vx) * dtMs / 1000);
    body.y += int32_t(int64_t(body.vy) * dtMs / 1000);
    Hits hits;
    hits.x = reflect(body.x, body.vx, maxX);
    hits.y = reflect(body.y, body.vy, maxY);
    return hits;
}
