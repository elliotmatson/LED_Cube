#include "sysinfo.h"

#include <stdio.h>
#include <string.h>

const char *sysinfo::resetReasonName(int reason)
{
    // In esp_reset_reason_t order.
    static const char *const REASONS[] = {"unknown", "power-on", "external", "software", "panic", "interrupt watchdog",
                                          "task watchdog", "other watchdog", "deep sleep", "brownout", "sdio", "usb",
                                          "jtag", "efuse", "power glitch", "cpu lockup"};
    return reason >= 0 && reason < int(sizeof(REASONS) / sizeof(REASONS[0])) ? REASONS[reason] : "unknown";
}

void sysinfo::formatUptime(uint32_t seconds, char *out, size_t size)
{
    const uint32_t days = seconds / 86400;
    const uint32_t hours = seconds / 3600 % 24;
    const uint32_t minutes = seconds / 60 % 60;
    const uint32_t secs = seconds % 60;
    if (days)
    {
        snprintf(out, size, "%lud %luh", (unsigned long)days, (unsigned long)hours);
    }
    else if (hours)
    {
        snprintf(out, size, "%luh %lum", (unsigned long)hours, (unsigned long)minutes);
    }
    else if (minutes)
    {
        snprintf(out, size, "%lum %lus", (unsigned long)minutes, (unsigned long)secs);
    }
    else
    {
        snprintf(out, size, "%lus", (unsigned long)secs);
    }
}

void sysinfo::formatBytes(uint32_t bytes, char *out, size_t size)
{
    if (bytes < 1024)
    {
        snprintf(out, size, "%lu B", (unsigned long)bytes);
    }
    else if (bytes < 1024UL * 1024)
    {
        snprintf(out, size, "%.1f KB", bytes / 1024.0);
    }
    else
    {
        snprintf(out, size, "%.2f MB", bytes / (1024.0 * 1024.0));
    }
}

bool sysinfo::parseIp(const char *text, char *out, size_t size)
{
    if (!text)
    {
        return false;
    }
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
    {
        text++;
    }
    size_t len = strlen(text);
    while (len && (text[len - 1] == ' ' || text[len - 1] == '\t' || text[len - 1] == '\r' || text[len - 1] == '\n'))
    {
        len--;
    }
    // "1.1.1.1" is the shortest IPv4 address and 45 characters the longest
    // IPv6 text form (with an embedded IPv4 address).
    if (len < 7 || len > 45 || len + 1 > size)
    {
        return false;
    }
    int dots = 0, colons = 0;
    bool hex = false;
    for (size_t i = 0; i < len; i++)
    {
        const char c = text[i];
        if (c == '.')
        {
            dots++;
        }
        else if (c == ':')
        {
            colons++;
        }
        else if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
        {
            hex = true;
        }
        else if (c < '0' || c > '9')
        {
            return false;
        }
    }
    if (colons == 1 || (colons == 0 && dots != 3))
    {
        return false;
    }
    if (colons == 0)
    {
        // IPv4: four decimal numbers up to 255.
        if (hex || len > 15)
        {
            return false;
        }
        unsigned a, b, c, d;
        char tail;
        char copy[16];
        memcpy(copy, text, len);
        copy[len] = '\0';
        if (sscanf(copy, "%3u.%3u.%3u.%3u%c", &a, &b, &c, &d, &tail) != 4 || a > 255 || b > 255 || c > 255 || d > 255)
        {
            return false;
        }
    }
    memcpy(out, text, len);
    out[len] = '\0';
    return true;
}
