#ifndef TIMEZONES_H
#define TIMEZONES_H

// Named time zones for the dashboard's dropdown, mapped to the POSIX TZ
// strings newlib's localtime() understands. Hardware independent; the rules
// are checked on the host (test/test_timezones).

#include <stddef.h>

namespace timezones
{
    struct Zone
    {
        const char *name;  // shown in the dropdown; no commas (the option separator)
        const char *posix; // for setenv("TZ") / configTzTime()
    };

    extern const Zone ZONES[];
    extern const size_t ZONE_COUNT;

    /// The zone used when none is saved, or the saved one is unknown.
    extern const Zone &DEFAULT_ZONE;

    /// Zone names joined with commas, in table order, for DropdownCard.
    const char *dropdownOptions();

    /// The zone with this name, or nullptr.
    const Zone *find(const char *name);
}

#endif
