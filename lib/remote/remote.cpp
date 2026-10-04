#include "remote.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace
{
    using remote::Setting;
    using remote::Type;

    // Ranges and lengths match what the dashboard allows.
    const remote::Spec SPECS[] = {
        {"pattern", Setting::PATTERN, Type::TEXT, 0, 0, 32},
        {"brightness", Setting::BRIGHTNESS, Type::NUMBER, 0, 255, 0},
        {"ticker", Setting::TICKER, Type::TEXT, 0, 0, 120}, // TICKER_MAX_LENGTH
        {"timezone", Setting::TIMEZONE, Type::TEXT, 0, 0, 48},
        {"weather_location", Setting::WEATHER_LOCATION, Type::TEXT, 0, 0, 60},
        {"weather_metric", Setting::WEATHER_METRIC, Type::FLAG, 0, 0, 0},
        {"github_updates", Setting::GITHUB_UPDATES, Type::FLAG, 0, 0, 0},
        {"development", Setting::DEVELOPMENT, Type::FLAG, 0, 0, 0},
        {"ota", Setting::OTA, Type::FLAG, 0, 0, 0},
        {"telemetry_interval", Setting::TELEMETRY_INTERVAL, Type::NUMBER, 60, 86400, 0},
        {"report_health", Setting::REPORT_HEALTH, Type::FLAG, 0, 0, 0},
        {"report_usage", Setting::REPORT_USAGE, Type::FLAG, 0, 0, 0},
        {"report_perf", Setting::REPORT_PERF, Type::FLAG, 0, 0, 0},
        // Lost mode: message and PIN first, then lost_mode true.
        {"lost_mode", Setting::LOST_MODE, Type::FLAG, 0, 0, 0},
        {"lost_silent", Setting::LOST_SILENT, Type::FLAG, 0, 0, 0},
        {"lost_message", Setting::LOST_MESSAGE, Type::TEXT, 0, 0, 120},
        {"lost_pin", Setting::LOST_PIN, Type::TEXT, 0, 0, 12, 4, true},
        {"restart", Setting::RESTART, Type::ACTION, 0, 0, 0},
        {"check_updates", Setting::CHECK_UPDATES, Type::ACTION, 0, 0, 0},
        {"resend_crash", Setting::RESEND_CRASH, Type::ACTION, 0, 0, 0},
    };

    bool equalsIgnoreCase(const std::string &a, const char *b)
    {
        if (a.size() != strlen(b))
        {
            return false;
        }
        for (size_t i = 0; i < a.size(); i++)
        {
            if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            {
                return false;
            }
        }
        return true;
    }
}

const remote::Spec *remote::specs(size_t &count)
{
    count = sizeof(SPECS) / sizeof(SPECS[0]);
    return SPECS;
}

const remote::Spec *remote::find(const char *name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    for (const Spec &s : SPECS)
    {
        if (strcmp(s.name, name) == 0)
        {
            return &s;
        }
    }
    return nullptr;
}

const char *remote::parse(const char *name, const char *payload, size_t length, Command &out)
{
    const Spec *spec = find(name);
    if (spec == nullptr)
    {
        return "unknown setting";
    }
    out = Command();
    out.id = spec->id;
    out.type = spec->type;
    if (spec->type == Type::ACTION)
    {
        return nullptr;
    }

    std::string value(payload ? payload : "", payload ? length : 0);
    // Trim, then unwrap a JSON string.
    while (!value.empty() && isspace((unsigned char)value.back()))
    {
        value.pop_back();
    }
    size_t start = 0;
    while (start < value.size() && isspace((unsigned char)value[start]))
    {
        start++;
    }
    value.erase(0, start);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    {
        value = value.substr(1, value.size() - 2);
    }

    switch (spec->type)
    {
    case Type::TEXT:
        if (value.size() > spec->maxLength)
        {
            return "too long";
        }
        // Empty is allowed whatever the minimum: it clears (a PIN, say).
        if (!value.empty() && value.size() < spec->minLength)
        {
            return "too short";
        }
        if (spec->digitsOnly)
        {
            for (char c : value)
            {
                if (c < '0' || c > '9')
                {
                    return "digits only";
                }
            }
        }
        if (value.find('\0') != std::string::npos)
        {
            return "contains a NUL";
        }
        out.text = value;
        return nullptr;
    case Type::FLAG:
        if (equalsIgnoreCase(value, "true") || equalsIgnoreCase(value, "on") || equalsIgnoreCase(value, "yes") || value == "1")
        {
            out.flag = true;
            return nullptr;
        }
        if (equalsIgnoreCase(value, "false") || equalsIgnoreCase(value, "off") || equalsIgnoreCase(value, "no") || value == "0")
        {
            out.flag = false;
            return nullptr;
        }
        return "not true or false";
    case Type::NUMBER:
    {
        if (value.empty() || value.size() > 12)
        {
            return "not a number";
        }
        char *end = nullptr;
        const long n = strtol(value.c_str(), &end, 10);
        if (end == nullptr || *end != '\0')
        {
            return "not a number";
        }
        if (n < spec->min || n > spec->max)
        {
            return "out of range";
        }
        out.number = n;
        return nullptr;
    }
    default:
        return nullptr;
    }
}
