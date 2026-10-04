#ifndef FIRE_H
#define FIRE_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"

/**
 * A fire burning up the side faces. The flames are the Doom fire on the
 * 128-pixel side strip: heat rises a row at a time, jittering sideways and
 * cooling at random, fed by a flickering bottom row. It runs at half
 * resolution, scaled up smoothly: bigger tongues, less pixel grain.
 * Embers fly up from the flame tips, fold over the top edge onto the top
 * face (lib/particles) and fade; the top face also catches a faint glow of
 * firelight along its edges.
 */
class Fire : public Pattern
{
public:
    Fire();
    ~Fire();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 33; }

    static const int STRIP_W = 2 * cube::FACE_SIZE;
    static const int STRIP_H = cube::FACE_SIZE;
    static const int SIM_W = STRIP_W / 2;
    static const int SIM_H = STRIP_H / 2;
    static const int EMBERS = 48;

private:
    struct Ember
    {
        cube::Vec3 p, v;
        float age = 0, life = 0; // seconds; dead when age >= life
    };
    uint32_t random();
    void spawnEmber(Ember &e);

    uint8_t *heat = nullptr; // SIM_W x SIM_H, row 0 at the top
    Ember embers[EMBERS];
    color::RGB palette[256];
    uint8_t columnGlow[SIM_W]; // how lit each column's top is, for the top face
    uint32_t rng = 1;
    uint32_t lastMs = 0;
    float spawnDebt = 0;
};

#endif
