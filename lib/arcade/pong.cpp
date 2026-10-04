#include "pong.h"

#include <math.h>

namespace
{
    /// Folds y into [0, span] the way bounces between two walls do.
    float fold(float y, float span)
    {
        if (span <= 0)
        {
            return 0;
        }
        float m = fmodf(y, 2 * span);
        if (m < 0)
        {
            m += 2 * span;
        }
        return m > span ? 2 * span - m : m;
    }

    /// Returns the ball off a paddle whose top is `paddleY`.
    void returnBall(const pong::Court &c, pong::Ball &b, float paddleY, float direction)
    {
        const float speed = fabsf(b.vx) * c.speedUp;
        // -1 at the paddle's top tip, +1 at its bottom.
        const float centre = paddleY + c.paddleHeight / 2;
        float offset = (b.y + c.ball / 2 - centre) / ((c.paddleHeight + c.ball) / 2);
        offset = offset < -1 ? -1 : (offset > 1 ? 1 : offset);
        b.vx = direction * speed;
        b.vy = offset * c.maxAngle * speed;
    }
}

pong::Event pong::step(const Court &c, Ball &b, float leftY, float rightY, float dt)
{
    const float x0 = b.x;
    b.x += b.vx * dt;
    b.y += b.vy * dt;

    Event event = Event::NONE;
    const float bottom = c.height - c.ball;
    if (b.y < 0 || b.y > bottom)
    {
        b.y = fold(b.y, bottom);
        b.vy = -b.vy;
        event = Event::WALL;
    }

    const float leftFace = c.paddleWidth;
    const float rightFace = c.width - c.paddleWidth - c.ball;
    auto overlaps = [&](float paddleY)
    { return b.y + c.ball > paddleY && b.y < paddleY + c.paddleHeight; };

    if (b.vx < 0 && x0 >= leftFace && b.x < leftFace)
    {
        if (overlaps(leftY))
        {
            b.x = 2 * leftFace - b.x;
            returnBall(c, b, leftY, +1);
            return Event::LEFT_HIT;
        }
    }
    if (b.vx > 0 && x0 <= rightFace && b.x > rightFace)
    {
        if (overlaps(rightY))
        {
            b.x = 2 * rightFace - b.x;
            returnBall(c, b, rightY, -1);
            return Event::RIGHT_HIT;
        }
    }
    if (b.x + c.ball < 0)
    {
        return Event::LEFT_MISS;
    }
    if (b.x > c.width)
    {
        return Event::RIGHT_MISS;
    }
    return event;
}

float pong::predictY(const Court &c, const Ball &b, float atX)
{
    if (b.vx == 0 || (atX - b.x) / b.vx < 0)
    {
        return b.y;
    }
    const float t = (atX - b.x) / b.vx;
    return fold(b.y + b.vy * t, c.height - c.ball);
}
