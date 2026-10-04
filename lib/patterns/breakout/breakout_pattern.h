#ifndef BREAKOUT_PATTERN_H
#define BREAKOUT_PATTERN_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"
#include "breakout.h"

/**
 * Breakout, playing itself on the side faces (one 128-pixel strip,
 * BottomPanels): a wall of rainbow bricks across the top, the paddle along
 * the bottom, and score, lives and level on the top face. The player hits
 * the ball with a different part of the paddle each time to spray it about
 * (lib/arcade), and now and then misjudges it. Clearing the wall builds a
 * faster one; losing three balls starts a new game.
 */
class BreakoutPattern : public Pattern
{
public:
    BreakoutPattern();
    ~BreakoutPattern();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 20; }

    static const int COLS = 16;
    static const int ROWS = 6;

private:
    enum class Phase : uint8_t
    {
        SERVE,
        PLAY,
        LOST,     // missed: a pause before the next ball
        CLEARED,  // the wall is down: a moment to admire it
        GAME_OVER,
    };
    float frand();
    void newGame();
    void newLevel();
    void serve();
    void chooseAim();
    void drawBrick(int i);
    void drawBricksNear(int16_t x, int16_t y);
    void drawPaddle();
    void drawBall(uint8_t level);
    void drawTop(const char *banner = nullptr);

    BottomPanels *strip = nullptr;
    SinglePanel *top = nullptr;
    breakout::Ball ball;
    uint8_t bricks[COLS * ROWS];
    float paddleX = 0, paddleDrawn = -1;
    float aimOffset = 0, aimError = 0;
    int score = 0, lives = 0, level = 0;
    Phase phase = Phase::SERVE;
    uint32_t phaseStartMs = 0, lastMs = 0;
    int16_t ballBoxX = 0, ballBoxY = 0;
    uint32_t rng = 1;
};

#endif
