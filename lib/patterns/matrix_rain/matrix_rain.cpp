#include "matrix_rain.h"

MatrixRain::MatrixRain()
{
    data.id = "matrix_rain";
    data.name = "Matrix Rain";
}

void MatrixRain::begin(PatternServices *services)
{
    pattern = services;
    for (Stream &s : streams)
    {
        spawn(s);
        // Stagger the start so the first frame is not one wall of drops.
        s.progress = random(0, 40000);
    }
    lastMs = millis();
}

void MatrixRain::spawn(Stream &s)
{
    // Seen from the shared corner, "down" on the top face is towards the two
    // near edges (+x into face 1, +y into face 2), and on the side faces it
    // is decreasing chain y (they are mounted rotated 180 degrees).
    switch (random(4))
    {
    case 0: // top face, heading for face 1
        s.head = {int16_t(random(0, 48)), int16_t(random(0, 64))};
        s.dir = cube::RIGHT;
        break;
    case 1: // top face, heading for face 2
        s.head = {int16_t(random(0, 64)), int16_t(random(0, 48))};
        s.dir = cube::DOWN;
        break;
    default: // top edge of a side face
        s.head = {int16_t(random(64, 192)), int16_t(63)};
        s.dir = cube::UP;
        break;
    }
    s.speed = uint16_t(random(12, 36));
    s.progress = 0;
    s.active = true;
}

void MatrixRain::tick()
{
    const uint32_t now = millis();
    const uint32_t dt = now - lastMs;
    lastMs = now;

    // Fade what is there: red and blue faster than green, so trails go from
    // the pale head to deep green.
    for (int16_t y = 0; y < cube::CHAIN_HEIGHT; y++)
    {
        uint8_t *p = pattern->display->rowForWrite(y, 0, cube::CHAIN_WIDTH);
        for (int16_t x = 0; x < cube::CHAIN_WIDTH; x++, p += 3)
        {
            p[0] = (p[0] * 22) >> 5;
            p[1] = (p[1] * 29) >> 5;
            p[2] = (p[2] * 22) >> 5;
        }
    }

    for (Stream &s : streams)
    {
        if (!s.active)
        {
            spawn(s);
        }
        s.progress += uint32_t(s.speed) * dt;
        while (s.progress >= 1000)
        {
            s.progress -= 1000;
            cube::Step next = cube::step(s.head, s.dir);
            if (!next.valid())
            {
                s.active = false; // off the bottom of a side face
                break;
            }
            s.head = next.to;
            s.dir = next.dir;
            pattern->display->drawPixelRGB888(s.head.x, s.head.y, 60, 255, 90);
        }
        if (s.active)
        {
            pattern->display->drawPixelRGB888(s.head.x, s.head.y, 200, 255, 200);
        }
    }
}
