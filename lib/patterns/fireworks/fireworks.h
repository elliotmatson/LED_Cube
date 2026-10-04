#ifndef FIREWORKS_H
#define FIREWORKS_H

#include <Arduino.h>
#include "cube_utils.h"
#include "color.h"

/**
 * Fireworks. Rockets launch from the bottom of the side faces and climb
 * against gravity, often folding over onto the top face, until their fuse
 * runs out and they burst into sparks of one colour. Sparks radiate across
 * whichever face they are on; on the top they slide outwards, fold over the
 * edges and fall down the sides (lib/particles). Each frame dims the last
 * rather than clearing it, so everything leaves trails.
 */
class Fireworks : public Pattern
{
public:
    Fireworks();
    ~Fireworks();
    void begin(PatternServices *services) override;
    void tick() override;
    void end() override;
    uint32_t frameInterval() const override { return 33; }

    static const int PARTICLES = 320;

private:
    struct Particle
    {
        cube::Vec3 p, v;
        float age = 0, life = 0; // seconds; free when age >= life
        color::RGB color;
        bool rocket = false;
    };
    uint32_t random();
    float frand();
    Particle *freeParticle();
    void launch();
    void burst(const Particle &rocket);

    Particle *particles = nullptr; // PSRAM
    uint32_t rng = 1;
    uint32_t lastMs = 0;
    float untilLaunch = 0;
};

#endif
