#ifndef BOUNCE_H
#define BOUNCE_H

// A body bouncing inside a rectangle, as the DVD screensaver's logo does.
// Hardware independent (test/test_bounce).

#include <stdint.h>

namespace bounce
{
    /// Fixed point: positions in 1/256 px, velocities in 1/256 px per second.
    struct Body
    {
        int32_t x = 0, y = 0;
        int32_t vx = 0, vy = 0;
    };

    /// Which walls a step bounced off.
    struct Hits
    {
        bool x = false; ///< the left or right wall
        bool y = false; ///< the top or bottom wall
        /// Both in the same step: the logo hit a corner.
        bool corner() const { return x && y; }
    };

    /**
     * Moves `body` for `dtMs` and reflects it off the walls of
     * [0, maxX] x [0, maxY] (maxX and maxY in 1/256 px: the room minus the
     * body's own size), reversing the velocity on each axis it hits.
     */
    Hits step(Body &body, int32_t maxX, int32_t maxY, uint32_t dtMs);
}

#endif
