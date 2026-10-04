#include "fireworks.h"

#include <math.h>
#include "particles.h"

namespace
{
    const cube::Vec3 GRAVITY = {0, 0, -26};
    // How much of the last frame survives into this one: the trails.
    const uint8_t TRAIL = 200; // of 256
    const float SPARK_DRAG = 0.75f; // per second
}

Fireworks::Fireworks()
{
    data.id = "fireworks";
    data.name = "Fireworks";
}

Fireworks::~Fireworks()
{
    end();
}

uint32_t Fireworks::random()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

float Fireworks::frand()
{
    return (random() & 0xFFFF) / 65535.0f;
}

void Fireworks::begin(PatternServices *services)
{
    pattern = services;
    particles = static_cast<Particle *>(heap_caps_calloc(PARTICLES, sizeof(Particle), MALLOC_CAP_SPIRAM));
    rng = esp_random() | 1;
    lastMs = millis();
    untilLaunch = 0.3f;
}

void Fireworks::end()
{
    free(particles);
    particles = nullptr;
}

Fireworks::Particle *Fireworks::freeParticle()
{
    for (int i = 0; i < PARTICLES; i++)
    {
        if (particles[i].age >= particles[i].life)
        {
            return &particles[i];
        }
    }
    return nullptr;
}

void Fireworks::launch()
{
    Particle *r = freeParticle();
    if (r == nullptr)
    {
        return;
    }
    // From the bottom of a side: strip x 4-123 is chain x 187-68, row 0.
    const int16_t sx = int16_t(4 + random() % 120);
    r->p = cube::toCube({int16_t(cube::CHAIN_WIDTH - 1 - sx), 0});
    r->v = {0, 0, 50.0f + frand() * 16.0f};
    // A little sideways, along whichever side face it is on.
    const float side = (frand() - 0.5f) * 10.0f;
    if (particles::faceAxis(r->p) == 0)
    {
        r->v.y = side;
    }
    else
    {
        r->v.x = side;
    }
    r->age = 0;
    r->life = 1.3f + frand() * 0.9f; // the fuse
    r->rocket = true;
    r->color = {255, 200, 120};
}

void Fireworks::burst(const Particle &rocket)
{
    const color::RGB base = color::hsv(uint8_t(random()), 220, 255);
    const int count = 40 + int(random() % 30);
    const int axis = particles::faceAxis(rocket.p);
    const float speed = 30.0f + frand() * 14.0f;
    for (int i = 0; i < count; i++)
    {
        Particle *s = freeParticle();
        if (s == nullptr)
        {
            return;
        }
        // A ring in the plane of the face it bursts on, with some spread.
        const float a = frand() * 2 * float(M_PI);
        const float v = speed * (0.55f + 0.45f * frand());
        const float c = cosf(a) * v, d = sinf(a) * v;
        s->p = rocket.p;
        s->v = axis == 0 ? cube::Vec3{0, c, d} : (axis == 1 ? cube::Vec3{c, 0, d} : cube::Vec3{c, d, 0});
        s->age = 0;
        s->life = 1.5f + frand() * 1.2f;
        s->rocket = false;
        // A touch of variety within the colour.
        s->color = color::lerp(base, {255, 255, 255}, uint8_t(random() % 60));
    }
}

void Fireworks::tick()
{
    if (particles == nullptr)
    {
        return;
    }
    const uint32_t now = millis();
    const float dt = min<uint32_t>(now - lastMs, 100) / 1000.0f;
    lastMs = now;

    Canvas &canvas = *pattern->display;
    // Dim last frame: the trails.
    for (int16_t y = 0; y < cube::CHAIN_HEIGHT; y++)
    {
        uint8_t *px = canvas.rowForWrite(y, 0, cube::CHAIN_WIDTH);
        for (int i = 0; i < cube::CHAIN_WIDTH * 3; i++)
        {
            px[i] = uint8_t((px[i] * TRAIL) >> 8);
        }
    }

    untilLaunch -= dt;
    if (untilLaunch <= 0)
    {
        launch();
        // Now and then two at once.
        if ((random() & 3) == 0)
        {
            launch();
        }
        untilLaunch = 0.5f + frand() * 1.1f;
    }

    for (int i = 0; i < PARTICLES; i++)
    {
        Particle &q = particles[i];
        if (q.age >= q.life)
        {
            continue;
        }
        q.age += dt;
        if (!q.rocket)
        {
            const float keep = 1.0f - SPARK_DRAG * dt;
            q.v.x *= keep;
            q.v.y *= keep;
            q.v.z *= keep;
        }
        const bool onSurface = particles::move(q.p, q.v, GRAVITY, dt);
        if (q.rocket && q.age >= q.life && onSurface)
        {
            burst(q);
            continue;
        }
        if (!onSurface)
        {
            q.age = q.life;
            continue;
        }
        const cube::Point p = cube::fromCube(q.p);
        if (!p.valid())
        {
            continue;
        }
        float level;
        if (q.rocket)
        {
            level = 0.9f;
        }
        else
        {
            // Bright, then fading; twinkling near the end.
            const float k = q.age / q.life;
            level = (1.0f - k) * (k > 0.6f && (random() & 1) ? 0.4f : 1.0f);
        }
        uint8_t *px = canvas.rowForWrite(p.y, p.x, 1);
        const uint8_t r = uint8_t(q.color.r * level), g = uint8_t(q.color.g * level), b = uint8_t(q.color.b * level);
        px[0] = max(px[0], r);
        px[1] = max(px[1], g);
        px[2] = max(px[2], b);
    }
}
