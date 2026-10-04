#ifndef PONG_PATTERN_H
#define PONG_PATTERN_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"
#include "pong.h"

/**
 * Pong, playing itself on the side faces (one 128-pixel strip,
 * BottomPanels), with the net down the corner seam between them and the
 * score on the top face. Each player aims where the ball will arrive
 * (lib/arcade), off by a little each rally and now and then by too much, so
 * points are won and lost. First to 11 wins; then a new game.
 */
class PongPattern : public Pattern
{
public:
    PongPattern();
    ~PongPattern();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 20; }

private:
    enum class Phase : uint8_t
    {
        SERVE, // a pause before the ball goes
        PLAY,
        WON,   // the game's over: celebrate, then start again
    };
    float frand();
    void serve(int towards);
    void newGame();
    void chooseError();
    float movePaddle(float y, float target, float dt);
    void drawPaddle(int16_t x, float y, color::RGB c);
    void drawBall(float x, float y, uint8_t level);
    void drawScore(uint32_t now);
    void drawNet(int16_t fromY, int16_t toY);

    BottomPanels *strip = nullptr;
    SinglePanel *top = nullptr;
    pong::Ball ball;
    float leftY = 0, rightY = 0;
    float leftError = 0, rightError = 0;
    int leftScore = 0, rightScore = 0;
    int lastWinner = 0; // -1 left, +1 right, for the flash on top
    Phase phase = Phase::SERVE;
    uint32_t phaseStartMs = 0, lastMs = 0, flashStartMs = 0;
    int16_t ballBoxX = 0, ballBoxY = 0; // last drawn, to clear
    // Paddle positions last drawn: a paddle is redrawn only when it moves,
    // since a column at each end of the strip would make every row's
    // changed span (and so the push) the strip's full width.
    float leftDrawn = -1, rightDrawn = -1;
    uint32_t rng = 1;
};

#endif
