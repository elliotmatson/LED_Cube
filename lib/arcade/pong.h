#ifndef PONG_H
#define PONG_H

// Pong's rules: the ball, the walls, the paddles and the score. Hardware
// independent (test/test_pong).

namespace pong
{
    /// The court: x from 0 (left) to width, y from 0 (top) to height.
    struct Court
    {
        float width, height;
        float ball;        // the ball's size; x, y is its top-left corner
        float paddleWidth; // paddles sit against the left and right edges
        float paddleHeight;
        float maxAngle;    // vertical speed / horizontal speed off a paddle's tip
        float speedUp;     // horizontal speed multiplier per hit
    };

    struct Ball
    {
        float x, y, vx, vy;
    };

    /// What a step did.
    enum class Event
    {
        NONE,
        WALL,       // off the top or bottom
        LEFT_HIT,   // returned by the left paddle
        RIGHT_HIT,
        LEFT_MISS,  // past the left paddle: a point to the right
        RIGHT_MISS,
    };

    /// Moves the ball for `dt`, bouncing it off the walls and the paddles
    /// (top edges `leftY` and `rightY`). A paddle returns the ball at an
    /// angle set by where it hit: square in the middle, steep at the tips.
    Event step(const Court &court, Ball &ball, float leftY, float rightY, float dt);

    /// The y the ball's top will have when it reaches x = `atX`, allowing
    /// for wall bounces on the way; the ball's current y if it is moving
    /// away.
    float predictY(const Court &court, const Ball &ball, float atX);
}

#endif
