#include "langton.h"

bool langton::parse(const char *text, Rule &rule)
{
    rule.states = 0;
    for (const char *c = text; *c; c++)
    {
        if (rule.states >= MAX_STATES || (*c != 'L' && *c != 'R' && *c != 'N' && *c != 'U'))
        {
            rule.states = 0;
            return false;
        }
        rule.turns[rule.states++] = *c;
    }
    return rule.states > 0;
}

int langton::step(const Rule &rule, uint8_t *grid, const int16_t *nbr, Ant &ant)
{
    const int cell = ant.cell;
    const uint8_t state = grid[cell] % rule.states;
    switch (rule.turns[state])
    {
    case 'R': ant.dir = uint8_t((ant.dir + 1) & 3); break;
    case 'L': ant.dir = uint8_t((ant.dir + 3) & 3); break;
    case 'U': ant.dir = uint8_t((ant.dir + 2) & 3); break;
    default: break;
    }
    grid[cell] = uint8_t((state + 1) % rule.states);

    const int to = nbr[cell * 4 + ant.dir];
    if (to < 0)
    {
        ant.dir = uint8_t((ant.dir + 2) & 3);
        return cell;
    }
    // Arriving heading: the opposite of the way back. Same as before on a
    // plain grid; turned across a rotated seam.
    for (int d = 0; d < 4; d++)
    {
        if (nbr[to * 4 + d] == cell)
        {
            ant.dir = uint8_t((d + 2) & 3);
            break;
        }
    }
    ant.cell = int16_t(to);
    return cell;
}
