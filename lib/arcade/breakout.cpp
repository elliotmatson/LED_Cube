#include "breakout.h"

#include <math.h>

namespace
{
    float fold(float x, float span)
    {
        if (span <= 0)
        {
            return 0;
        }
        float m = fmodf(x, 2 * span);
        if (m < 0)
        {
            m += 2 * span;
        }
        return m > span ? 2 * span - m : m;
    }

    /// The standing brick under point (x, y), or -1.
    int brickAt(const breakout::Field &f, const uint8_t *bricks, float x, float y)
    {
        if (y < f.brickTop || x < 0 || x >= f.width)
        {
            return -1;
        }
        const int row = int((y - f.brickTop) / f.brickH), col = int(x / f.brickW);
        if (row < 0 || row >= f.rows || col < 0 || col >= f.cols)
        {
            return -1;
        }
        // The cell's last column and row are the gap between bricks.
        const float inX = x - col * f.brickW, inY = y - f.brickTop - row * f.brickH;
        if (inX >= f.brickW - 1 || inY >= f.brickH - 1)
        {
            return -1;
        }
        const int i = row * f.cols + col;
        return bricks[i] ? i : -1;
    }
}

breakout::Result breakout::step(const Field &f, Ball &b, uint8_t *bricks, float paddleX, float dt)
{
    const float x0 = b.x, y0 = b.y;
    b.x += b.vx * dt;
    b.y += b.vy * dt;
    Result r;

    const float right = f.width - f.ball;
    if (b.x < 0 || b.x > right)
    {
        b.x = fold(b.x, right);
        b.vx = -b.vx;
        r.event = Event::WALL;
    }
    if (b.y < 0)
    {
        b.y = -b.y;
        b.vy = -b.vy;
        r.event = Event::WALL;
    }

    // Bricks: check the ball's four corners.
    const float cx[4] = {b.x, b.x + f.ball - 0.01f, b.x, b.x + f.ball - 0.01f};
    const float cy[4] = {b.y, b.y, b.y + f.ball - 0.01f, b.y + f.ball - 0.01f};
    for (int k = 0; k < 4; k++)
    {
        const int hit = brickAt(f, bricks, cx[k], cy[k]);
        if (hit < 0)
        {
            continue;
        }
        bricks[hit] = 0;
        // Which side it came in from: if the ball already overlapped the
        // brick's columns last step, it came through the top or bottom.
        const int col = hit % f.cols;
        const float left = col * f.brickW, rightEdge = left + f.brickW - 1;
        const bool overlappedX = x0 + f.ball > left && x0 < rightEdge;
        if (overlappedX)
        {
            b.vy = -b.vy;
            b.y = y0;
        }
        else
        {
            b.vx = -b.vx;
            b.x = x0;
        }
        b.vx *= f.speedUp;
        b.vy *= f.speedUp;
        r.event = Event::BRICK;
        r.brick = hit;
        return r;
    }

    // The paddle, on the way down.
    if (b.vy > 0 && y0 + f.ball <= f.paddleY && b.y + f.ball > f.paddleY &&
        b.x + f.ball > paddleX && b.x < paddleX + f.paddleWidth)
    {
        const float speed = sqrtf(b.vx * b.vx + b.vy * b.vy);
        float offset = (b.x + f.ball / 2 - (paddleX + f.paddleWidth / 2)) / ((f.paddleWidth + f.ball) / 2);
        offset = offset < -1 ? -1 : (offset > 1 ? 1 : offset);
        const float a = offset * f.maxAngle;
        b.vx = speed * sinf(a);
        b.vy = -speed * cosf(a);
        b.y = f.paddleY - f.ball;
        r.event = Event::PADDLE;
        return r;
    }
    if (b.y > f.height)
    {
        r.event = Event::MISS;
    }
    return r;
}

float breakout::predictX(const Field &f, const Ball &b, float atY)
{
    if (b.vy <= 0)
    {
        return b.x;
    }
    const float t = (atY - b.y) / b.vy;
    return fold(b.x + b.vx * t, f.width - f.ball);
}

int breakout::remaining(const Field &f, const uint8_t *bricks)
{
    int n = 0;
    for (int i = 0; i < f.rows * f.cols; i++)
    {
        n += bricks[i] != 0;
    }
    return n;
}
