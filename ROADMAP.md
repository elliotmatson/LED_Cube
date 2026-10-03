# LED Cube modernization plan

Working plan agreed 2026-10-03. Decisions: GitHub-hosted CI runners for now;
ElegantOTA replaced by an upload card; ESP-DASH Pro v5.1.0 kept; dashboard left
open on the LAN (as on hub); phased PRs.

## Done / in review

- **#46 — CI and platform.** pio-actions callers (build-release, static-analysis),
  dependency updater + Dependabot, pioarduino 55.03.312-1 (IDF 5.5.5 / Arduino
  3.3.12), AsyncTCP 3.5.0 / ESPAsyncWebServer 3.12.1, `sdkconfig.defaults`,
  unused managed components removed, GitHub updater finds `<env>-<tag>.bin`.
- **#47 — upload card and fixes.** Dashboard firmware upload with image checks,
  app rollback, core dumps to flash, serialized pattern switching, safe
  `stop()`, Plasma yield, Spotify use-after-free fixes, WiFi portal with a
  random password and timeout, brightness API validation, Spotify login via
  the HTTPS relay. Tested on hardware.
- **#48 — Spotify login relay page** on GitHub Pages.
- **spotify-api-arduino-psram#2** — token buffer overflows, secret logging.

## PR 3 — architecture

1. **Host unit tests first**, as in lp-p2p: `[env:native]`, Unity suites under
   `test/`, and `test.yml` calling pio-actions. Pull hardware-independent logic
   into libraries without `Arduino.h`: face/seam adjacency, `SinglePanel`
   rotation maps, the `PROJ_CALC_*` projection, a cube-surface 3D mapping,
   Game of Life step, Snake board/moves, firmware version comparison, prefs
   validation and migration.
2. **One render loop.** A single render task pinned to core 1 at a fixed frame
   rate drives patterns through `begin(Context&)` / `tick(Canvas&, dt)` /
   `end()`. No pattern owns or kills a task; I/O-bound patterns (Spotify) get a
   background worker that hands state to `tick` under a lock.
3. **Double buffering** (`double_buff` + `flipDMABuffer`). Clock's flicker is
   already gone (frames reach the panels whole). What remains is tearing while
   a full-frame push (~24 ms) runs across ~2 panel refreshes. A second DMA
   buffer at 8-bit colour is another ~98 KB of internal RAM, and only ~86 KB
   is free, so it means 6-bit colour (~74 KB a buffer, ~37 KB left free) or
   PSRAM DMA buffers. Needs eyes on the panels to judge whether it is worth it;
   making the push faster (below) shrinks the tear window either way.
4. **Pattern registry** with stable string ids; the selected pattern is saved by
   id, not index. Patterns declare their settings; one layer builds both the
   dashboard cards and `/api/v1/patterns`.
5. **Split `Cube`** into display, network/time, updates, settings, pattern
   manager and web UI/API. Time zone from a POSIX TZ string set on the
   dashboard, replacing the plain-HTTP worldtimeapi.org lookup (fails today, so
   the clock runs in UTC).
6. Debounce NVS writes from sliders.

## Performance

- **Investigate ESP32-S3 SIMD (PIE, the 128-bit vector extension).** Measured with
  the render loop (`/api/v1/stats`, 2026-10-03): pushing a full frame costs
  ~24 ms (~2 us a pixel in the HUB75 library's `drawPixelRGB888`); ticks cost
  Snake ~9-11 ms, Plasma ~12.7 ms, Game of Life ~5.5 ms. Frame rates: Snake
  ~29 fps, Plasma ~26, Game of Life 20 (its interval). The push is the first
  target: write bit planes for a whole row at a time instead of per pixel,
  then vectorize that.
  Candidates: per-pixel pattern maths (Plasma's field, blends, fades), the
  RGB888 framebuffer to HUB75 bit-plane conversion, image scaling for album
  art. Tools: esp-dsp (already a managed component, with S3-optimized `_aes3`
  routines), hand-written `ee.*` vector instructions for hot loops. GCC does not
  auto-vectorize for PIE, so gains need explicit code. Keep a scalar fallback
  and test both against each other in the native suite.
- `-O2` for release builds; Snake to integer/`float` maths and dirty-cell
  redraws; Spotify polling every 1–2 s instead of 300 ms.

## Patterns

- Fix existing: Clock (large digits across the side faces via `BottomPanels`),
  Game of Life (all three faces via seam adjacency, reseed on stagnation),
  Snakes (carry direction across seams).
- New, built on the 3D mapping: 3D noise/fire volume, plane sweeps, falling sand
  toward the shared corner, corner ripples, Matrix rain, rotating wireframes,
  Rubik's scramble/solve, information/scrolling text.

## Spotify (deferred until the plan above is done)

- **Client ID on the dashboard**, so release builds (which have no `secrets.h`)
  can log in.
- **PKCE run in the browser on the relay page.** The browser redeems the code with
  its own verifier and hands the refresh token to the cube in the URL fragment;
  no client secret on the cube. Needs the fork to persist rotated refresh
  tokens. Until then the client secret must stay: with the relay forwarding to
  any private host, a code is only safe because it cannot be redeemed without
  the secret.
- Relay hardening: state expiry (~10 min), 128-bit state, strict state format
  check on the relay page; comment in `config.h` that the secret must never ship
  in published builds.
- Setup guide: creating the Spotify app (development mode: Premium owner,
  5 allowlisted users), registering the relay redirect URI.
- Optional: a Home Assistant "now playing" pattern as a source-agnostic
  alternative.
- Check Chrome's Local Network Access rules against the relay's https → LAN
  navigation.

## Bugs noticed

- The pattern buttons on the dashboard come up in a different order on each
  boot.

## Later

- Tag the TJpg_Decoder and spotify forks so the dependency updater can track
  them; consider `espressif/esp_jpeg` instead of the TJpg fork.
- Hold IDF 6 / Arduino 4 until HUB75-MatrixPanel-DMA supports it.
