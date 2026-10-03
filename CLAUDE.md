# LED Cube — working notes

ESP32-S3 firmware for a cube of three 64×64 HUB75 LED panels sharing one
corner. It runs animated patterns, serves an ESP-DASH Pro dashboard and a small
REST API, and updates itself from GitHub releases.

## Build

```sh
pio run                                   # build esp32-s3-devkitc-1
pio run -t upload                         # flash over the network (espota, cube.local)
pio run -t monitor                        # serial monitor, 115200
pio run -t save-defconfig                 # regenerate sdkconfig.defaults
pio run -t idf-size                       # size summary (also size-components, size-files)
```

If `pio` is not on `PATH`, it is at `~/.platformio/penv/bin/pio`.

The project builds against **both** ESP-IDF and the Arduino core
(`framework = espidf, arduino`) on the pioarduino platform, so IDF and Arduino
APIs both appear throughout. The platform pin and PlatformIO Core move
together: pioarduino 55.03.312 and later need PlatformIO >= 6.2.0, which is
why CI passes `pio-version: latest`.

Uploads default to ArduinoOTA at `cube.local`, which has to be switched on in
the dashboard first (a fresh cube has it off). To flash over USB, comment out
`upload_protocol` and `upload_port` in `platformio.ini`, as the README describes.

## Configuration

`sdkconfig.defaults` is the source of truth. `sdkconfig.esp32-s3-devkitc-1` is
generated from it on every build and is gitignored. To change a setting, run
`pio run -t menuconfig`, then `pio run -t save-defconfig`, and diff before
committing — save-defconfig strips comments.

`lib/cube/config.h` holds pins, panel geometry and build identity.
`FW_VERSION`, `FW_TYPE` and `REPO_URL` default to `"DEV"` and this repo; CI
injects the real values. A `"DEV"` build never auto-updates from GitHub, so a
locally flashed image is not replaced by the latest release.

`lib/cube/secrets.h` (gitignored, optional) can define `SPOTIFY_CLIENT_ID`
and `SPOTIFY_CLIENT_SECRET` for local builds.

## Layout

```
src/main.cpp                 setup() calls Cube::init(); loop() deletes itself
lib/cube/                    the Cube class: display, WiFi, prefs, dashboard, API, OTA, GitHub updates
lib/cube_utils/              Pattern base class, SinglePanel/BottomPanels virtual displays, projection macros
lib/patterns/<name>/         one folder per pattern; registered in lib/patterns/all_patterns.cpp
lib/fonts/                   GFX fonts
api/                         Bruno collection for the REST API
```

Physical panel *p* of the 192×64 chain is `x ∈ [64p, 64p+63]`. `SinglePanel`
gives a rotated 64×64 view of one face; `BottomPanels` a 128×64 upright strip
across the two side faces; `PROJ_CALC_*` an isometric projection that is
continuous across all three seams.

## Things that will bite you

- **ESP-DASH-Pro is a private repository.** Its commercial license forbids
  publishing the source, so CI clones it with `GH_PAT`. Do not vendor it here
  or make the fork public.
- **Release asset names.** pio-actions publishes `esp32-s3-devkitc-1-<tag>.bin`.
  The updater (`Cube::findFirmwareRelease`) finds it through the releases API.
  `build-release.yml` also publishes `esp32s3.bin` for cubes still running
  the pre-pio-actions updater; remove it once none are left.
- **Unused managed components.** arduino-esp32's manifest pulls in RainMaker,
  Insights, Zigbee, speech recognition and more. `custom_component_remove` in
  `platformio.ini` drops them. If a platform bump brings back a build error
  about `https_server.crt` or `rmaker_*` certs, a new component has slipped
  in: add it to that list rather than embedding its certs.
- **Patterns own FreeRTOS tasks.** Each pattern's `start()` creates a task and
  `stop()` deletes it. Clear the handle after deleting and never call
  `stop()` on a pattern that is not running.

## CI

Callers of the shared workflows in `elliotmatson/pio-actions@v1`:
`build-release.yml` (PRs and manual releases: stable / beta / build),
`static-analysis.yml` (`pio check`, fails on high), plus
`dependency-updates.yml` (weekly `platformio.ini` bumps through
`elliotmatson/platformio-dependency-updater`) and Dependabot for actions.
`docs.yml` publishes Doxygen to GitHub Pages from `main`.

## Conventions

- Doxygen comments on public functions and headers.
- Non-obvious code carries a comment explaining *why*.
- 4-space indent, Allman braces, matching what is already there.
