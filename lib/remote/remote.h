#ifndef REMOTE_H
#define REMOTE_H

// The settings and actions a cube accepts from its MQTT broker
// (cube/<id>/set/<name>), and the checks every value passes before it is
// applied. Hardware independent (test/test_remote).

#include <stddef.h>
#include <string>

namespace remote
{
    enum class Setting : unsigned char
    {
        PATTERN,
        BRIGHTNESS,
        TICKER,
        TIMEZONE,
        WEATHER_LOCATION,
        WEATHER_METRIC,
        GITHUB_UPDATES,
        DEVELOPMENT,
        OTA,
        TELEMETRY_INTERVAL,
        REPORT_HEALTH,
        REPORT_USAGE,
        REPORT_PERF,
        LOST_MODE,
        LOST_SILENT,
        LOST_MESSAGE,
        LOST_PIN,
        RESTART,
        CHECK_UPDATES,
        RESEND_CRASH,
    };

    enum class Type : unsigned char
    {
        TEXT,
        NUMBER,
        FLAG,
        ACTION, // the payload is ignored
    };

    struct Spec
    {
        const char *name; // the topic suffix
        Setting id;
        Type type;
        long min, max;    // NUMBER: inclusive range
        size_t maxLength; // TEXT
        size_t minLength = 0;    // TEXT, unless empty (which clears it)
        bool digitsOnly = false; // TEXT: 0-9 only (a PIN)
    };

    struct Command
    {
        Setting id;
        Type type;
        long number = 0;
        bool flag = false;
        std::string text;
    };

    /// Every setting and action, for documentation and lookups.
    const Spec *specs(size_t &count);

    /// The spec named `name`, or nullptr.
    const Spec *find(const char *name);

    /**
     * Parses a command: `name` is the topic after ".../set/", `payload` the
     * message (not NUL-terminated). Text may be sent bare or as a JSON
     * string ("..."); flags as true/false, on/off, yes/no or 1/0; numbers as
     * decimal integers within the setting's range.
     *
     * @return nullptr on success, otherwise why it was rejected.
     */
    const char *parse(const char *name, const char *payload, size_t length, Command &out);
}

#endif
