#ifndef LOST_MODE_H
#define LOST_MODE_H

#include <Arduino.h>
#include <Preferences.h>
#include <functional>
#include <string>

#include "settings.h"

/**
 * The parts of lost mode that are not the cube's UI: the unlock PIN and the
 * search for a way back online. (Cube::setLost turns it on and off, locks
 * the dashboard and shows the Lost pattern; Telemetry reports the cube's
 * location while it lasts.)
 *
 * The PIN is kept only as a salted SHA-256, and guesses are rate-limited
 * across restarts: after five wrong ones, one try every fifteen minutes.
 *
 * While lost and offline for a few minutes -- and not showing the WiFi
 * setup portal, which is the likeliest way back online -- it tries the
 * strongest open networks in range, keeping one only if the broker can be
 * reached through it. The saved WiFi credentials are never overwritten.
 */
class LostMode
{
public:
    /// `brokerConnected` says whether telemetry has a connection.
    void begin(Settings &settings, std::function<bool()> brokerConnected);

    /// Stores a new PIN (as a salted hash); empty removes it.
    void setPin(const std::string &pin);
    bool hasPin() const;

    enum class Unlock
    {
        OK,
        WRONG,
        LOCKED_OUT, // too many wrong guesses: wait
        NO_PIN,     // none set: only MQTT can clear lost mode
        NOT_LOST,
    };
    /// Checks a PIN typed on the dashboard. Does not clear lost mode itself.
    Unlock tryUnlock(const std::string &pin, uint32_t &waitSeconds);

    /// How the cube is online: "saved" network or "open:<ssid>".
    String via() const { return _via; }

private:
    void task();
    bool tryOpenNetworks();
    static std::string hash(const std::string &salt, const std::string &pin);

    Settings *settings = nullptr;
    std::function<bool()> brokerConnected;
    Preferences prefs;
    uint32_t failures = 0;
    uint32_t lastFailureMs = 0;
    uint32_t offlineSinceMs = 0;
    String _via = "saved";
};

#endif
