#ifndef BOOT_LOG_H
#define BOOT_LOG_H

#include <Arduino.h>
#include <Preferences.h>

/**
 * The last few boots, kept in NVS, so a failed startup or a rollback can be
 * explained afterwards without a serial console.
 *
 * Each boot records the firmware version, how the previous boot ended (the
 * reset reason this one started with), whether it was the first boot of a
 * fresh update, and how far startup got. A boot after an update that never
 * reached BOOT_CONFIRMED is the one the bootloader rolled back; its stage says
 * where it stopped, and the next boot's reset reason says why.
 */
class BootLog
{
public:
    /// How far startup got, in order.
    enum Stage : uint8_t
    {
        BOOT_STARTED,
        BOOT_SETTINGS,
        BOOT_DISPLAY,
        BOOT_WIFI,
        BOOT_PATTERNS,
        BOOT_API,
        BOOT_UI,
        BOOT_UPDATES,
        BOOT_CONFIRMED,
    };

    struct Entry
    {
        char version[28];
        uint8_t reason;      // esp_reset_reason_t this boot started with
        uint8_t stage;       // Stage reached
        uint8_t afterUpdate; // the first boot of a fresh image
        uint8_t used;
    };
    static const int ENTRIES = 8;

    /// Records this boot. Call first thing, before anything that might
    /// reset; also notes an image the bootloader rolled back.
    void begin();
    /// Records progress through startup (saved at once: the next thing may
    /// be what resets).
    void reached(Stage stage);

    /// Entry i, 0 the current boot, 1 the one before...; nullptr past the end.
    const Entry *entry(int i) const;
    /// The version of an image the bootloader rejected, or "".
    const char *rolledBackFrom() const { return _rolledBackFrom; }
    /// One line on how the last startup went, for the dashboard.
    String summary() const;

    static const char *reasonName(uint8_t reason);
    static const char *stageName(uint8_t stage);

private:
    void save();

    Preferences _prefs;
    bool _ready = false;
    struct Ring
    {
        uint8_t head; // index of the current boot's entry
        Entry entries[ENTRIES];
    } _ring = {};
    char _rolledBackFrom[32] = "";
};

#endif
