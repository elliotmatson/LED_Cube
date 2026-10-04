#include "boot_log.h"

#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

namespace
{
    const char *K_RING = "ring";
}

void BootLog::begin()
{
    _ready = _prefs.begin("boots");
    if (_ready && _prefs.getBytesLength(K_RING) == sizeof(_ring))
    {
        _prefs.getBytes(K_RING, &_ring, sizeof(_ring));
    }
    else
    {
        memset(&_ring, 0, sizeof(_ring));
    }

    _ring.head = uint8_t((_ring.head + 1) % ENTRIES);
    Entry &e = _ring.entries[_ring.head];
    memset(&e, 0, sizeof(e));
    strlcpy(e.version, esp_app_get_description()->version, sizeof(e.version));
    e.reason = uint8_t(esp_reset_reason());
    e.stage = BOOT_STARTED;
    esp_ota_img_states_t state;
    e.afterUpdate = esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
                    state == ESP_OTA_IMG_PENDING_VERIFY;
    e.used = 1;
    save();

    // An image the bootloader gave up on (it reset before confirming).
    const esp_partition_t *invalid = esp_ota_get_last_invalid_partition();
    esp_app_desc_t desc;
    if (invalid && esp_ota_get_partition_description(invalid, &desc) == ESP_OK)
    {
        strlcpy(_rolledBackFrom, desc.version, sizeof(_rolledBackFrom));
    }

    const Entry *previous = entry(1);
    ESP_LOGI("Boot", "%s, started by %s%s", e.version, reasonName(e.reason), e.afterUpdate ? ", first boot of an update" : "");
    if (previous && previous->stage != BOOT_CONFIRMED)
    {
        ESP_LOGW("Boot", "The previous boot (%s) stopped at %s, then: %s", previous->version, stageName(previous->stage),
                 reasonName(e.reason));
    }
    if (_rolledBackFrom[0])
    {
        ESP_LOGW("Boot", "Rolled back from %s", _rolledBackFrom);
    }
}

void BootLog::reached(Stage stage)
{
    Entry &e = _ring.entries[_ring.head];
    if (e.stage != stage)
    {
        e.stage = stage;
        save();
    }
}

void BootLog::save()
{
    if (_ready)
    {
        _prefs.putBytes(K_RING, &_ring, sizeof(_ring));
    }
}

const BootLog::Entry *BootLog::entry(int i) const
{
    if (i < 0 || i >= ENTRIES)
    {
        return nullptr;
    }
    const Entry &e = _ring.entries[(_ring.head + ENTRIES - i) % ENTRIES];
    return e.used ? &e : nullptr;
}

String BootLog::summary() const
{
    const Entry *now = entry(0), *previous = entry(1);
    String s;
    if (_rolledBackFrom[0])
    {
        s = String("Rolled back from ") + _rolledBackFrom + ". ";
    }
    if (previous && previous->stage != BOOT_CONFIRMED)
    {
        s += String("The boot before this one (") + previous->version + ") stopped at " + stageName(previous->stage) +
             "; it ended in a " + reasonName(now ? now->reason : 0) + " reset.";
    }
    else if (now)
    {
        s += String("Started normally (") + reasonName(now->reason) + ")";
    }
    return s;
}

const char *BootLog::reasonName(uint8_t reason)
{
    static const char *const NAMES[] = {"unknown", "power-on", "external", "software", "panic", "interrupt watchdog",
                                        "task watchdog", "other watchdog", "deep sleep", "brownout", "sdio", "usb",
                                        "jtag", "efuse", "power glitch", "cpu lockup"};
    return reason < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[reason] : "unknown";
}

const char *BootLog::stageName(uint8_t stage)
{
    static const char *const NAMES[] = {"start", "settings", "display", "wifi", "patterns", "api", "dashboard", "updates",
                                        "confirmed"};
    return stage < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[stage] : "unknown";
}
