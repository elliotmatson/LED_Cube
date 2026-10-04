#ifndef BREAKOUT_H
#define BREAKOUT_H

// Breakout's rules: the ball, the walls, the paddle along the bottom and a
// wall of bricks at the top. Hardware independent (test/test_breakout).

#include <stdint.h>

namespace breakout
{
    /// The field: x from 0 to width, y from 0 (top) to height.
    struct Field
    {
        float width, height;
        float ball;          // size; x, y is its top-left corner
        float paddleY;       // the paddle's top
        float paddleWidth, paddleHeight;
        float maxAngle;      // radians from straight up off a paddle's tip
        float speedUp;       // speed multiplier per brick
        int cols, rows;      // the brick wall
        float brickTop;      // y of the first row
        float brickW, brickH; // cell size, gap included
    };

    struct Ball
    {
        float x, y, vx, vy;
    };

    enum class Event
    {
        NONE,
        WALL,
        PADDLE,
        BRICK,
        MISS, // past the paddle
    };

    struct Result
    {
        Event event = Event::NONE;
        int brick = -1; // row * cols + col, for BRICK
    };

    /**
     * Moves the ball for `dt`. It bounces off the side walls and the top,
     * off the paddle (left edge at `paddleX`) at an angle set by where it
     * hits, and off bricks, breaking one: `bricks` has rows * cols entries,
     * non-zero for a brick still standing, and the one hit is zeroed.
     */
    Result step(const Field &field, Ball &ball, uint8_t *bricks, float paddleX, float dt);

    /// The x the ball's left edge will have when its top reaches `atY`,
    /// allowing for side-wall bounces (not bricks); its current x if it is
    /// moving up.
    float predictX(const Field &field, const Ball &ball, float atY);

    /// Bricks still standing.
    int remaining(const Field &field, const uint8_t *bricks);
}

#endif
