#include "wordclock.h"

#include <string.h>

namespace wordclock
{
    const char *const GRID[ROWS] = {
        "ITKISAHALFBTENRX",
        "QUARTERXTWENTYPX",
        "FIVEZMINUTESYTOL",
        "PASTONETWOTHREEW",
        "FOURFIVESIXSEVEN",
        "EIGHTNINETENDGHS",
        "ELEVENTWELVEJMKV",
        "AMWQHOCLOCKBNFPM",
    };
}

namespace
{
    struct Word
    {
        uint8_t row, col, len;
    };
    const Word IT = {0, 0, 2}, IS = {0, 3, 2}, HALF = {0, 6, 4}, TEN_M = {0, 11, 3};
    const Word QUARTER = {1, 0, 7}, TWENTY = {1, 8, 6};
    const Word FIVE_M = {2, 0, 4}, MINUTES = {2, 5, 7}, TO = {2, 13, 2};
    const Word PAST = {3, 0, 4};
    const Word HOURS[12] = {
        {6, 6, 6}, // twelve
        {3, 4, 3}, // one
        {3, 7, 3}, // two
        {3, 10, 5}, // three
        {4, 0, 4}, // four
        {4, 4, 4}, // five
        {4, 8, 3}, // six
        {4, 11, 5}, // seven
        {5, 0, 5}, // eight
        {5, 5, 4}, // nine
        {5, 9, 3}, // ten
        {6, 0, 6}, // eleven
    };
    const Word OCLOCK = {7, 5, 6}, AM = {7, 0, 2}, PM = {7, 14, 2};

    void mark(const Word &w, uint8_t *lit)
    {
        for (int i = 0; i < w.len; i++)
        {
            lit[w.row * wordclock::COLS + w.col + i] = 1;
        }
    }

    /// The words for a time, in reading order (grid order), into `out`.
    int words(int hour, int minute, Word *out)
    {
        int n = 0;
        out[n++] = IT;
        out[n++] = IS;
        const int five = (minute / 5) * 5;
        // From twenty-five to, the hour is the next one.
        const int h = (five > 30 ? hour + 1 : hour) % 12;
        switch (five)
        {
        case 0: break;
        case 5: out[n++] = FIVE_M; out[n++] = MINUTES; out[n++] = PAST; break;
        case 10: out[n++] = TEN_M; out[n++] = MINUTES; out[n++] = PAST; break;
        case 15: out[n++] = QUARTER; out[n++] = PAST; break;
        case 20: out[n++] = TWENTY; out[n++] = MINUTES; out[n++] = PAST; break;
        case 25: out[n++] = TWENTY; out[n++] = FIVE_M; out[n++] = MINUTES; out[n++] = PAST; break;
        case 30: out[n++] = HALF; out[n++] = PAST; break;
        case 35: out[n++] = TWENTY; out[n++] = FIVE_M; out[n++] = MINUTES; out[n++] = TO; break;
        case 40: out[n++] = TWENTY; out[n++] = MINUTES; out[n++] = TO; break;
        case 45: out[n++] = QUARTER; out[n++] = TO; break;
        case 50: out[n++] = TEN_M; out[n++] = MINUTES; out[n++] = TO; break;
        default: out[n++] = FIVE_M; out[n++] = MINUTES; out[n++] = TO; break;
        }
        out[n++] = HOURS[h];
        if (five == 0)
        {
            out[n++] = OCLOCK;
        }
        // Morning or afternoon of the hour named (11:40 is "twenty to
        // twelve" -- PM, since it is noon that is meant).
        const int named = five > 30 ? (hour + 1) % 24 : hour;
        out[n++] = named < 12 ? AM : PM;
        // Grid order.
        for (int i = 1; i < n; i++)
        {
            for (int j = i; j > 0 && (out[j].row * 16 + out[j].col) < (out[j - 1].row * 16 + out[j - 1].col); j--)
            {
                const Word t = out[j];
                out[j] = out[j - 1];
                out[j - 1] = t;
            }
        }
        return n;
    }
}

void wordclock::light(int hour, int minute, uint8_t *lit)
{
    memset(lit, 0, COLS * ROWS);
    Word w[12];
    const int n = words(hour, minute, w);
    for (int i = 0; i < n; i++)
    {
        mark(w[i], lit);
    }
}

char *wordclock::phrase(int hour, int minute, char *out, int size)
{
    Word w[12];
    const int n = words(hour, minute, w);
    int at = 0;
    for (int i = 0; i < n && at < size - 1; i++)
    {
        if (i > 0)
        {
            out[at++] = ' ';
        }
        for (int k = 0; k < w[i].len && at < size - 1; k++)
        {
            out[at++] = GRID[w[i].row][w[i].col + k];
        }
    }
    out[at] = '\0';
    return out;
}
