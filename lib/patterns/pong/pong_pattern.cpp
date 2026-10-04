#include "pong_pattern.h"

#include <Fonts/FreeSansBold18pt7b.h>
#include <math.h>

namespace
{
    const pong::Court COURT = {128, 64, 2, 2, 12, 0.85f, 1.04f};
    const float SERVE_SPEED = 42.0f;
    const float MAX_SPEED = 110.0f;
    const float PADDLE_SPEED = 55.0f; // px/s
    const uint32_t SERVE_MS = 1000;
    const uint32_t WON_MS = 4000;
    const uint32_t FLASH_MS = 800;
    const int WINNING_SCORE = 11;
    const color::RGB LEFT_COLOR = {60, 150, 255};
    const color::RGB RIGHT_COLOR = {255, 90, 50};
}

PongPattern::PongPattern()
{
    data.id = "pong";
    data.name = "Pong";
}

PongPattern::~PongPattern()
{
    end();
}

float PongPattern::frand()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (rng & 0xFFFF) / 65535.0f;
}

void PongPattern::begin(PatternServices *services)
{
    pattern = services;
    strip = new BottomPanels(*pattern->display);
    top = new SinglePanel(*pattern->display, 0, 0);
    rng = esp_random() | 1;
    lastMs = millis();
    newGame();
}

void PongPattern::end()
{
    delete strip;
    delete top;
    strip = nullptr;
    top = nullptr;
}

void PongPattern::newGame()
{
    leftScore = rightScore = 0;
    lastWinner = 0;
    leftY = rightY = (COURT.height - COURT.paddleHeight) / 2;
    leftDrawn = rightDrawn = -1;
    pattern->display->fillScreen(0);
    drawNet(0, int16_t(COURT.height) - 1);
    drawScore(millis());
    serve(frand() < 0.5f ? -1 : 1);
}

/// Picks how far off each player's aim will be this rally: usually a little,
/// sometimes enough to miss.
void PongPattern::chooseError()
{
    auto pick = [this]()
    {
        const float r = frand();
        const float size = r < 0.18f ? 9.0f + frand() * 6.0f : frand() * 4.0f;
        return frand() < 0.5f ? -size : size;
    };
    leftError = pick();
    rightError = pick();
}

void PongPattern::serve(int towards)
{
    ball.x = (COURT.width - COURT.ball) / 2;
    ball.y = 8 + frand() * (COURT.height - 16);
    ball.vx = towards * SERVE_SPEED;
    ball.vy = (frand() - 0.5f) * SERVE_SPEED;
    chooseError();
    phase = Phase::SERVE;
    phaseStartMs = millis();
}

float PongPattern::movePaddle(float y, float target, float dt)
{
    target = constrain(target, 0.0f, COURT.height - COURT.paddleHeight);
    const float step = PADDLE_SPEED * dt;
    if (fabsf(target - y) <= step)
    {
        return target;
    }
    return y + (target > y ? step : -step);
}

/// A paddle at strip column x (two wide), its top at y, edges anti-aliased.
void PongPattern::drawPaddle(int16_t x, float y, color::RGB c)
{
    for (int16_t sy = 0; sy < COURT.height; sy++)
    {
        // How much of pixel row sy the paddle covers.
        const float cover = max(0.0f, min(float(sy + 1), y + COURT.paddleHeight) - max(float(sy), y));
        const uint8_t k = uint8_t(255 * min(cover, 1.0f));
        const color::RGB p = color::scale(c, k);
        strip->drawPixelRGB888(x, sy, p.r, p.g, p.b);
        strip->drawPixelRGB888(x + 1, sy, p.r, p.g, p.b);
    }
}

/// The ball, a 2x2 square at a fractional position, as 3x3 pixels weighted
/// by how much of each it covers.
void PongPattern::drawBall(float x, float y, uint8_t level)
{
    const int16_t ix = int16_t(floorf(x)), iy = int16_t(floorf(y));
    for (int16_t j = 0; j < 3; j++)
    {
        const float cy = max(0.0f, min(float(iy + j + 1), y + COURT.ball) - max(float(iy + j), y));
        for (int16_t i = 0; i < 3; i++)
        {
            const float cx = max(0.0f, min(float(ix + i + 1), x + COURT.ball) - max(float(ix + i), x));
            const uint8_t v = uint8_t(level * min(cx * cy, 1.0f));
            if (v > 0)
            {
                strip->drawPixelRGB888(ix + i, iy + j, v, v, v);
            }
        }
    }
    ballBoxX = ix;
    ballBoxY = iy;
}

/// The net down the corner seam, rows fromY to toY.
void PongPattern::drawNet(int16_t fromY, int16_t toY)
{
    for (int16_t y = max<int16_t>(fromY, 0); y <= min<int16_t>(toY, int16_t(COURT.height) - 1); y++)
    {
        const uint8_t v = (y & 3) < 2 ? 70 : 0;
        strip->drawPixelRGB888(63, y, v, v, v);
        strip->drawPixelRGB888(64, y, v, v, v);
    }
}

/// The score on top, each in its player's colour, over a fading flash of
/// whoever just scored.
void PongPattern::drawScore(uint32_t now)
{
    uint8_t flash = 0;
    if (lastWinner != 0 && now - flashStartMs < FLASH_MS)
    {
        flash = uint8_t(90 * (1.0f - float(now - flashStartMs) / FLASH_MS));
    }
    const color::RGB bg = color::scale(lastWinner < 0 ? LEFT_COLOR : RIGHT_COLOR, flash);
    top->fillScreen(Canvas::color565(bg.r, bg.g, bg.b));
    top->setFont(&FreeSansBold18pt7b);
    top->setTextSize(1);
    char text[4];
    int16_t x1, y1;
    uint16_t w, h;
    snprintf(text, sizeof(text), "%d", leftScore);
    top->getTextBounds(text, 0, 44, &x1, &y1, &w, &h);
    top->setTextColor(Canvas::color565(LEFT_COLOR.r, LEFT_COLOR.g, LEFT_COLOR.b));
    top->setCursor(16 - int16_t(w) / 2 - x1, 44);
    top->print(text);
    snprintf(text, sizeof(text), "%d", rightScore);
    top->getTextBounds(text, 0, 44, &x1, &y1, &w, &h);
    top->setTextColor(Canvas::color565(RIGHT_COLOR.r, RIGHT_COLOR.g, RIGHT_COLOR.b));
    top->setCursor(48 - int16_t(w) / 2 - x1, 44);
    top->print(text);
    top->setFont(NULL);
    // A dotted divider.
    for (int16_t y = 14; y < 50; y += 4)
    {
        top->drawPixel(31, y, Canvas::color565(90, 90, 90));
        top->drawPixel(32, y, Canvas::color565(90, 90, 90));
    }
}

void PongPattern::tick()
{
    if (strip == nullptr)
    {
        return;
    }
    const uint32_t now = millis();
    const float dt = min<uint32_t>(now - lastMs, 100) / 1000.0f;
    lastMs = now;

    // Clear last frame's ball, mending the net if it was over it.
    strip->fillRect(ballBoxX, ballBoxY, 3, 3, 0);
    if (ballBoxX <= 64 && ballBoxX + 2 >= 63)
    {
        drawNet(ballBoxY, ballBoxY + 2);
    }

    switch (phase)
    {
    case Phase::SERVE:
        if (now - phaseStartMs >= SERVE_MS)
        {
            phase = Phase::PLAY;
        }
        break;
    case Phase::PLAY:
    {
        const pong::Event e = pong::step(COURT, ball, leftY, rightY, dt);
        if (e == pong::Event::LEFT_HIT || e == pong::Event::RIGHT_HIT)
        {
            // Cap the rally's speed, and pick new aims for the return.
            const float speed = fabsf(ball.vx);
            if (speed > MAX_SPEED)
            {
                ball.vx *= MAX_SPEED / speed;
                ball.vy *= MAX_SPEED / speed;
            }
            chooseError();
        }
        else if (e == pong::Event::LEFT_MISS || e == pong::Event::RIGHT_MISS)
        {
            const bool rightScored = e == pong::Event::LEFT_MISS;
            (rightScored ? rightScore : leftScore)++;
            lastWinner = rightScored ? 1 : -1;
            flashStartMs = now;
            if (leftScore >= WINNING_SCORE || rightScore >= WINNING_SCORE)
            {
                phase = Phase::WON;
                phaseStartMs = now;
            }
            else
            {
                serve(rightScored ? -1 : 1); // towards whoever lost the point
            }
        }
        break;
    }
    case Phase::WON:
        if (now - phaseStartMs >= WON_MS)
        {
            newGame();
            return;
        }
        // Keep flashing the winner.
        if (now - flashStartMs >= FLASH_MS)
        {
            flashStartMs = now;
        }
        break;
    }

    // Paddles: the one the ball is heading for goes to meet it; the other
    // drifts back to the middle.
    const float centre = (COURT.height - COURT.paddleHeight) / 2;
    const float meet = COURT.ball / 2 - COURT.paddleHeight / 2;
    if (phase == Phase::PLAY && ball.vx < 0)
    {
        leftY = movePaddle(leftY, pong::predictY(COURT, ball, COURT.paddleWidth) + meet + leftError, dt);
        rightY = movePaddle(rightY, centre, dt);
    }
    else if (phase == Phase::PLAY)
    {
        rightY = movePaddle(rightY, pong::predictY(COURT, ball, COURT.width - COURT.paddleWidth - COURT.ball) + meet + rightError, dt);
        leftY = movePaddle(leftY, centre, dt);
    }
    if (leftY != leftDrawn)
    {
        drawPaddle(0, leftY, LEFT_COLOR);
        leftDrawn = leftY;
    }
    if (rightY != rightDrawn)
    {
        drawPaddle(int16_t(COURT.width - 2), rightY, RIGHT_COLOR);
        rightDrawn = rightY;
    }

    if (phase != Phase::WON)
    {
        drawBall(ball.x, ball.y, phase == Phase::SERVE ? uint8_t(120 + 135 * ((now / 150) & 1)) : 255);
    }
    if (lastWinner != 0 && now - flashStartMs <= FLASH_MS + 40)
    {
        drawScore(now);
    }
}
