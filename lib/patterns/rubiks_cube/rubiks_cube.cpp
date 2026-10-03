#include "rubiks_cube.h"

namespace
{
    const uint32_t MOVE_MS = 300;
    const uint32_t PAUSE_MS[] = {0, 2000, 0, 3000}; // after each phase

    // The three visible faces, as directions in cube::toCube space: the top
    // face is z = 64, face 1 is x = 64, face 2 is y = 64.
    const int8_t NORMALS[3][3] = {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}};

    const uint8_t PALETTE[6][3] = {
        {210, 210, 210}, // white
        {255, 210, 0},   // yellow
        {220, 0, 0},     // red
        {255, 90, 0},    // orange
        {0, 190, 40},    // green
        {0, 60, 255},    // blue
    };

    uint32_t rnd() { return esp_random(); }

    // Which of the three rows a coordinate in [0, 64] falls in (-1, 0, 1),
    // and whether it is in the gap between stickers.
    int8_t cell(float c, bool &gap)
    {
        const float size = 64.0f / 3;
        int i = int(c / size);
        i = i < 0 ? 0 : (i > 2 ? 2 : i);
        const float local = c - i * size;
        gap = gap || local < 1.2f || local > size - 1.2f;
        return int8_t(i - 1);
    }
}

RubiksCube::RubiksCube()
{
    data.id = "rubiks_cube";
    data.name = "Rubik's Cube";
}

RubiksCube::~RubiksCube()
{
    end();
}

void RubiksCube::begin(PatternServices *services)
{
    pattern = services;
    slots = static_cast<uint8_t *>(heap_caps_malloc(cube::CELLS, MALLOC_CAP_SPIRAM));
    if (!slots)
    {
        return;
    }
    for (int i = 0; i < cube::CELLS; i++)
    {
        const cube::Point p{int16_t(i % cube::CHAIN_WIDTH), int16_t(i / cube::CHAIN_WIDTH)};
        const int face = cube::face(p.x);
        const cube::Vec3 v = cube::toCube(p);
        const float c[3] = {v.x, v.y, v.z};
        const int axis = face == 0 ? 2 : (face == 1 ? 0 : 1);
        const int u = (axis + 1) % 3, w = (axis + 2) % 3;
        bool gap = false;
        const int8_t a = cell(c[u], gap), b = cell(c[w], gap);
        slots[i] = gap ? GAP : uint8_t(face * 9 + (a + 1) * 3 + (b + 1));
    }
    model.reset();
    phase = SOLVED;
    phaseStartMs = millis();
    draw();
}

void RubiksCube::end()
{
    free(slots);
    slots = nullptr;
}

void RubiksCube::draw()
{
    // The colour of each of the 27 visible stickers, then every pixel.
    uint8_t colors[27];
    for (int face = 0; face < 3; face++)
    {
        const int axis = face == 0 ? 2 : (face == 1 ? 0 : 1);
        const int u = (axis + 1) % 3, w = (axis + 2) % 3;
        for (int8_t a = -1; a <= 1; a++)
        {
            for (int8_t b = -1; b <= 1; b++)
            {
                int8_t pos[3];
                pos[axis] = 1;
                pos[u] = a;
                pos[w] = b;
                colors[face * 9 + (a + 1) * 3 + (b + 1)] = model.at(pos, NORMALS[face]);
            }
        }
    }
    const uint8_t *slot = slots;
    for (int16_t y = 0; y < cube::CHAIN_HEIGHT; y++)
    {
        uint8_t *out = pattern->display->rowForWrite(y, 0, cube::CHAIN_WIDTH);
        for (int16_t x = 0; x < cube::CHAIN_WIDTH; x++, slot++)
        {
            const uint8_t *rgb = *slot == GAP ? (const uint8_t *)"\0\0\0" : PALETTE[colors[*slot]];
            *out++ = rgb[0];
            *out++ = rgb[1];
            *out++ = rgb[2];
        }
    }
}

void RubiksCube::tick()
{
    if (!slots)
    {
        return;
    }
    const uint32_t now = millis();
    if (phase == SCRAMBLED || phase == SOLVED)
    {
        if (now - phaseStartMs < PAUSE_MS[phase])
        {
            return;
        }
        phase = phase == SOLVED ? SCRAMBLING : SOLVING;
        moveIndex = phase == SCRAMBLING ? 0 : SCRAMBLE_MOVES - 1;
        lastMoveMs = now - MOVE_MS;
    }
    if (now - lastMoveMs < MOVE_MS)
    {
        return;
    }
    lastMoveMs = now;
    if (phase == SCRAMBLING)
    {
        moves[moveIndex] = rubiks::randomMove(moveIndex ? &moves[moveIndex - 1] : nullptr, rnd);
        model.apply(moves[moveIndex]);
        if (++moveIndex == SCRAMBLE_MOVES)
        {
            phase = SCRAMBLED;
            phaseStartMs = now;
        }
    }
    else
    {
        model.apply(moves[moveIndex].inverse());
        if (--moveIndex < 0)
        {
            phase = SOLVED;
            phaseStartMs = now;
        }
    }
    draw();
}
