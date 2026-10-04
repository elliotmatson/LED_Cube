#ifndef AQUARIUM_H
#define AQUARIUM_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"

/**
 * An aquarium. The side faces are the tank, as one 128-pixel strip: deep
 * water with slanting light, sand and swaying seaweed, and pixel-art fish
 * swimming back and forth at their own depths and speeds. Bubbles rise from
 * the sand, fold over the top edge (lib/particles) and pop into ripples on
 * the top face, which shows the water's surface from above with moving
 * caustics.
 */
class Aquarium : public Pattern
{
public:
    Aquarium();
    ~Aquarium();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 33; }

    static const int STRIP_W = 2 * cube::FACE_SIZE;
    static const int STRIP_H = cube::FACE_SIZE;
    static const int FISH = 7;
    static const int BUBBLES = 40;
    static const int RIPPLES = 12;
    static const int WEEDS = 7;

private:
    struct Fish
    {
        int kind;
        float x, y, speed, phase;
        int dir; // -1 left, +1 right
    };
    struct Bubble
    {
        cube::Vec3 p, v;
        bool alive = false;
        float wobble;
    };
    struct Ripple
    {
        float x, y, age;
        bool alive = false;
    };
    struct Weed
    {
        int16_t x, height;
        float phase;
        color::RGB color;
    };

    uint32_t random();
    float frand();
    void drawStrip(int sx, int sy, color::RGB c);
    void drawFish(const Fish &f);
    void drawTop(float t);

    uint8_t *background = nullptr; // the strip's water and sand, RGB888, PSRAM
    Fish fish[FISH];
    Bubble bubbles[BUBBLES];
    Ripple ripples[RIPPLES];
    Weed weeds[WEEDS];
    uint32_t rng = 1;
    uint32_t startMs = 0, lastMs = 0;
    float bubbleDebt = 0;
};

#endif
