#include "breakout_pattern.h"

#include <Fonts/FreeSansBold12pt7b.h>
#include <math.h>

namespace
{
    const breakout::Field FIELD = {128, 64, 2, 60, 14, 2, 1.05f, 1.006f, BreakoutPattern::COLS, BreakoutPattern::ROWS, 4, 8, 3};
    const float BASE_SPEED = 48.0f;
    const float LEVEL_SPEED = 6.0f;
    const float MAX_SPEED = 95.0f;
    const float PADDLE_SPEED = 80.0f;
    const int LIVES = 3;
    const uint32_t SERVE_MS = 1000, LOST_MS = 1200, CLEARED_MS = 2500, GAME_OVER_MS = 3500;
}

BreakoutPattern::BreakoutPattern()
{
    data.id = "breakout";
    data.name = "Breakout";
}

BreakoutPattern::~BreakoutPattern()
{
    end();
}

float BreakoutPattern::frand()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (rng & 0xFFFF) / 65535.0f;
}

void BreakoutPattern::begin(PatternServices *services)
{
    pattern = services;
    strip = new BottomPanels(*pattern->display);
    top = new SinglePanel(*pattern->display, 0, 0);
    rng = esp_random() | 1;
    lastMs = millis();
    newGame();
}

void BreakoutPattern::end()
{
    delete strip;
    delete top;
    strip = nullptr;
    top = nullptr;
}

void BreakoutPattern::newGame()
{
    score = 0;
    lives = LIVES;
    level = 0;
    newLevel();
}

void BreakoutPattern::newLevel()
{
    level++;
    memset(bricks, 1, sizeof(bricks));
    pattern->display->fillScreen(0);
    for (int i = 0; i < COLS * ROWS; i++)
    {
        drawBrick(i);
    }
    paddleX = (FIELD.width - FIELD.paddleWidth) / 2;
    paddleDrawn = -1;
    drawTop();
    serve();
}

void BreakoutPattern::serve()
{
    ball.x = paddleX + FIELD.paddleWidth / 2 - FIELD.ball / 2;
    ball.y = FIELD.paddleY - FIELD.ball;
    const float speed = min(MAX_SPEED, BASE_SPEED + LEVEL_SPEED * (level - 1));
    const float a = (frand() - 0.5f) * 1.2f;
    ball.vx = speed * sinf(a);
    ball.vy = -speed * cosf(a);
    chooseAim();
    phase = Phase::SERVE;
    phaseStartMs = millis();
}

/// Where on the paddle to take the next return, and how wrong to get it:
/// mostly a few pixels, now and then enough to miss.
void BreakoutPattern::chooseAim()
{
    aimOffset = (frand() - 0.5f) * 1.3f; // of half the paddle
    aimError = frand() < 0.07f ? (frand() < 0.5f ? -1 : 1) * (10 + frand() * 8) : (frand() - 0.5f) * 3;
}

void BreakoutPattern::drawBrick(int i)
{
    const int row = i / COLS, col = i % COLS;
    const int16_t x = int16_t(col * FIELD.brickW), y = int16_t(FIELD.brickTop + row * FIELD.brickH);
    const color::RGB c = bricks[i] ? color::hsv(uint8_t(row * 36), 255, 220) : color::RGB{0, 0, 0};
    strip->fillRect(x, y, int16_t(FIELD.brickW - 1), int16_t(FIELD.brickH - 1), Canvas::color565(c.r, c.g, c.b));
}

/// Redraws standing bricks under a 3x3 box at (x, y), which clearing the
/// ball may have nicked.
void BreakoutPattern::drawBricksNear(int16_t x, int16_t y)
{
    for (int16_t dy = 0; dy < 3; dy += 2)
    {
        for (int16_t dx = 0; dx < 3; dx += 2)
        {
            const float py = y + dy - FIELD.brickTop;
            if (py < 0)
            {
                continue;
            }
            const int row = int(py / FIELD.brickH), col = int((x + dx) / FIELD.brickW);
            if (row >= 0 && row < ROWS && col >= 0 && col < COLS && bricks[row * COLS + col])
            {
                drawBrick(row * COLS + col);
            }
        }
    }
}

void BreakoutPattern::drawPaddle()
{
    const int16_t y = int16_t(FIELD.paddleY);
    strip->fillRect(0, y, int16_t(FIELD.width), int16_t(FIELD.paddleHeight), 0);
    // Anti-aliased ends.
    for (int16_t x = int16_t(floorf(paddleX)); x <= int16_t(paddleX + FIELD.paddleWidth); x++)
    {
        const float cover = max(0.0f, min(float(x + 1), paddleX + FIELD.paddleWidth) - max(float(x), paddleX));
        const uint8_t v = uint8_t(230 * min(cover, 1.0f));
        for (int16_t j = 0; j < FIELD.paddleHeight; j++)
        {
            strip->drawPixelRGB888(x, y + j, v, v, v);
        }
    }
    paddleDrawn = paddleX;
}

void BreakoutPattern::drawBall(uint8_t level)
{
    const int16_t ix = int16_t(floorf(ball.x)), iy = int16_t(floorf(ball.y));
    for (int16_t j = 0; j < 3; j++)
    {
        const float cy = max(0.0f, min(float(iy + j + 1), ball.y + FIELD.ball) - max(float(iy + j), ball.y));
        for (int16_t i = 0; i < 3; i++)
        {
            const float cx = max(0.0f, min(float(ix + i + 1), ball.x + FIELD.ball) - max(float(ix + i), ball.x));
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

/// Level, score and lives on top, or a banner instead of the score.
void BreakoutPattern::drawTop(const char *banner)
{
    top->fillScreen(0);
    top->setFont(NULL);
    top->setTextSize(1);
    top->setTextColor(Canvas::color565(150, 150, 150));
    char text[16];
    snprintf(text, sizeof(text), "LEVEL %d", level);
    top->setCursor(int16_t((64 - 6 * int(strlen(text))) / 2), 6);
    top->print(text);
    if (banner)
    {
        top->setTextColor(Canvas::color565(255, 80, 60));
        top->setCursor(int16_t((64 - 6 * int(strlen(banner))) / 2), 28);
        top->print(banner);
    }
    else
    {
        snprintf(text, sizeof(text), "%d", score);
        int16_t x1, y1;
        uint16_t w, h;
        top->setFont(&FreeSansBold12pt7b);
        top->getTextBounds(text, 0, 40, &x1, &y1, &w, &h);
        top->setTextColor(0xFFFF);
        top->setCursor((64 - int16_t(w)) / 2 - x1, 40);
        top->print(text);
        top->setFont(NULL);
    }
    for (int i = 0; i < LIVES; i++)
    {
        const uint16_t c = i < lives ? Canvas::color565(255, 255, 255) : Canvas::color565(40, 40, 40);
        top->fillRect(int16_t(22 + i * 8), 52, 3, 3, c);
    }
}

void BreakoutPattern::tick()
{
    if (strip == nullptr)
    {
        return;
    }
    const uint32_t now = millis();
    const float dt = min<uint32_t>(now - lastMs, 100) / 1000.0f;
    lastMs = now;

    strip->fillRect(ballBoxX, ballBoxY, 3, 3, 0);
    drawBricksNear(ballBoxX, ballBoxY);

    switch (phase)
    {
    case Phase::SERVE:
        // The ball rides on the paddle until it goes.
        ball.x = paddleX + FIELD.paddleWidth / 2 - FIELD.ball / 2;
        if (now - phaseStartMs >= SERVE_MS)
        {
            phase = Phase::PLAY;
        }
        break;
    case Phase::PLAY:
    {
        const breakout::Result r = breakout::step(FIELD, ball, bricks, paddleX, dt);
        if (r.event == breakout::Event::BRICK)
        {
            drawBrick(r.brick);
            score += 10 * (ROWS - r.brick / COLS);
            drawTop();
            // Keep it from running away.
            const float speed = sqrtf(ball.vx * ball.vx + ball.vy * ball.vy);
            if (speed > MAX_SPEED)
            {
                ball.vx *= MAX_SPEED / speed;
                ball.vy *= MAX_SPEED / speed;
            }
            if (breakout::remaining(FIELD, bricks) == 0)
            {
                phase = Phase::CLEARED;
                phaseStartMs = now;
                drawTop("CLEARED!");
            }
        }
        else if (r.event == breakout::Event::PADDLE)
        {
            chooseAim();
        }
        else if (r.event == breakout::Event::MISS)
        {
            lives--;
            phase = lives > 0 ? Phase::LOST : Phase::GAME_OVER;
            phaseStartMs = now;
            drawTop(lives > 0 ? "MISS" : "GAME OVER");
        }
        break;
    }
    case Phase::LOST:
        if (now - phaseStartMs >= LOST_MS)
        {
            drawTop();
            serve();
        }
        break;
    case Phase::CLEARED:
        if (now - phaseStartMs >= CLEARED_MS)
        {
            newLevel();
            return;
        }
        break;
    case Phase::GAME_OVER:
        if (now - phaseStartMs >= GAME_OVER_MS)
        {
            newGame();
            return;
        }
        break;
    }

    // The paddle: under the ball's landing point when it is coming down,
    // taking it on the chosen part of the paddle; otherwise following it.
    float target;
    if (phase == Phase::PLAY && ball.vy > 0)
    {
        const float land = breakout::predictX(FIELD, ball, FIELD.paddleY - FIELD.ball);
        target = land + FIELD.ball / 2 - FIELD.paddleWidth / 2 - aimOffset * FIELD.paddleWidth / 2 + aimError;
    }
    else
    {
        target = ball.x + FIELD.ball / 2 - FIELD.paddleWidth / 2;
    }
    target = constrain(target, 0.0f, FIELD.width - FIELD.paddleWidth);
    if (phase == Phase::PLAY || phase == Phase::SERVE)
    {
        const float step = PADDLE_SPEED * dt;
        paddleX = fabsf(target - paddleX) <= step ? target : paddleX + (target > paddleX ? step : -step);
    }
    if (paddleX != paddleDrawn)
    {
        drawPaddle();
    }
    if (phase == Phase::SERVE || phase == Phase::PLAY)
    {
        drawBall(255);
    }
}
