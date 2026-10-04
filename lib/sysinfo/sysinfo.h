#ifndef SYSINFO_H
#define SYSINFO_H

// Formatting for the dashboard's statistics and /api/v1/stats: uptime, byte
// counts, reset reasons, and checking a public IP lookup's answer.
// Hardware independent (test/test_sysinfo).

#include <stddef.h>
#include <stdint.h>

namespace sysinfo
{
    /// A short name for an esp_reset_reason_t value ("power-on", "panic",
    /// "task watchdog"); "unknown" for anything out of range.
    const char *resetReasonName(int reason);

    /// Uptime as "5s", "12m 5s", "4h 12m" or "3d 4h": the two largest units.
    void formatUptime(uint32_t seconds, char *out, size_t size);

    /// A byte count as "512 B", "123.4 KB" or "7.81 MB" (1 KB = 1024 B).
    void formatBytes(uint32_t bytes, char *out, size_t size);

    /// Copies the IP address in a lookup service's plain-text answer into
    /// `out`, trimming surrounding whitespace. False, leaving `out` alone,
    /// unless the answer is a bare IPv4 or IPv6 address -- so an HTML error
    /// page or a captive portal never shows up as the cube's address.
    bool parseIp(const char *text, char *out, size_t size);
}

#endif
