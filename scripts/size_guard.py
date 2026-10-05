"""Fails the build when firmware.bin would not fit an older cube's app slot.

Some cubes in the field still have the partition table from before
min_spiffs_s3.csv: Arduino's min_spiffs.csv, whose app slots are 0x1E0000
bytes. A partition table only changes over USB, so those cubes keep it through
every OTA update, and an image larger than the slot fails at 99% with
ESP_ERR_INVALID_SIZE -- the upload card, ArduinoOTA and the GitHub updater
alike. v0.6.0 was 17 KB over and could not reach them.

Keep this until no cube with the old table is left.
"""

Import("env")

import os

OLD_APP_SLOT = 0x1E0000  # app0/app1 in min_spiffs.csv


def check_size(source, target, env):
    path = target[0].get_abspath()
    size = os.path.getsize(path)
    margin = OLD_APP_SLOT - size
    if margin < 0:
        print(
            "\n*** %s is %d bytes, %d over the %d-byte app slot that cubes with the "
            "old min_spiffs.csv partition table have. They could not install it. "
            "See scripts/size_guard.py.\n" % (os.path.basename(path), size, -margin, OLD_APP_SLOT)
        )
        env.Exit(1)
    print("Old-layout app slot: %d bytes free (image %d of %d)" % (margin, size, OLD_APP_SLOT))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", check_size)
