#include "timezones.h"

#include <string.h>

namespace timezones
{
    // Rules as of 2026. US: second Sunday of March to first Sunday of
    // November. EU: last Sunday of March to last Sunday of October, at 01:00
    // UTC.
    const Zone ZONES[] = {
        {"UTC", "UTC0"},
        {"US Eastern", "EST5EDT,M3.2.0,M11.1.0"},
        {"US Central", "CST6CDT,M3.2.0,M11.1.0"},
        {"US Mountain", "MST7MDT,M3.2.0,M11.1.0"},
        {"US Arizona", "MST7"},
        {"US Pacific", "PST8PDT,M3.2.0,M11.1.0"},
        {"US Alaska", "AKST9AKDT,M3.2.0,M11.1.0"},
        {"US Hawaii", "HST10"},
        {"Brazil Sao Paulo", "<-03>3"},
        {"UK and Ireland", "GMT0BST,M3.5.0/1,M10.5.0"},
        {"Central Europe", "CET-1CEST,M3.5.0,M10.5.0/3"},
        {"Eastern Europe", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
        {"India", "IST-5:30"},
        {"China", "CST-8"},
        {"Japan", "JST-9"},
        {"Australia Eastern", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
        {"New Zealand", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
    };
    const size_t ZONE_COUNT = sizeof(ZONES) / sizeof(ZONES[0]);
    const Zone &DEFAULT_ZONE = ZONES[0];

    const char *dropdownOptions()
    {
        static char options[512];
        if (options[0] == '\0')
        {
            for (size_t i = 0; i < ZONE_COUNT; i++)
            {
                if (i > 0)
                {
                    strncat(options, ",", sizeof(options) - strlen(options) - 1);
                }
                strncat(options, ZONES[i].name, sizeof(options) - strlen(options) - 1);
            }
        }
        return options;
    }

    const Zone *find(const char *name)
    {
        if (name == nullptr)
        {
            return nullptr;
        }
        for (size_t i = 0; i < ZONE_COUNT; i++)
        {
            if (strcmp(ZONES[i].name, name) == 0)
            {
                return &ZONES[i];
            }
        }
        return nullptr;
    }
}
