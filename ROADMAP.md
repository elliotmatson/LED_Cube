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
5. Done: `Cube` split into `Renderer` (render task, canvas, switching),
   `Updates` (all three update paths) and `Settings`; `cube.cpp` keeps
   startup, display, WiFi/time, dashboard and API (1,282 -> 564 lines).
6. Done in the settings PR: per-key settings with debounced writes and
   migration, time zone from a dashboard dropdown (POSIX rules, replacing
   worldtimeapi.org), pattern saved by id, deterministic dashboard card order.

## Performance

- **Investigate ESP32-S3 SIMD (PIE, the 128-bit vector extension).** Measured with
  the render loop (`/api/v1/stats`, 2026-10-03): pushing a full frame costs
  ~24 ms (~2 us a pixel in the HUB75 library's `drawPixelRGB888`); ticks cost
  Snake ~9-11 ms, Plasma ~12.7 ms, Game of Life ~5.5 ms. Frame rates: Snake
  ~29 fps, Plasma ~26, Game of Life 20 (its interval).
  - Done: `drawRowRGB888()` in the elliotmatson fork of the HUB75 library
    (row pointers looked up once per run) and `-O2`: push 24 -> 17 ms, Plasma
    34.5 fps, Snake 38.4. The fork is temporary -- draft an upstream PR to
    mrcodetastic/ESP32-HUB75-MatrixPanel-DMA, then return to upstream.
  - What is left is memory-bound: ~98k read-modify-writes of 16-bit DMA
    words per frame. PIE could do eight words per 128-bit load/store
    (`ee.vld.128` / `ee.andq` / `ee.orq` / `ee.vst.128`), which also cuts bus
    transactions eightfold; needs 16-byte-aligned runs with scalar edges.
    Worth it only if a pattern needs more than ~35 fps.
  Candidates: per-pixel pattern maths (Plasma's field, blends, fades), the
  RGB888 framebuffer to HUB75 bit-plane conversion, image scaling for album
  art. Tools: esp-dsp (already a managed component, with S3-optimized `_aes3`
  routines), hand-written `ee.*` vector instructions for hot loops. GCC does not
  auto-vectorize for PIE, so gains need explicit code. Keep a scalar fallback
  and test both against each other in the native suite.
- Done: `-O2`; Plasma's projection precomputed and rows written directly
  (tick 10.7 -> 2.45 ms, 47.6 fps, animation now time-based); Snake on
  `float` with direct row writes (7.7 -> 5.0 ms, paced at 30 steps/s);
  `fast_cos` inline; Spotify polls every second.
- SIMD (PIE) is integer-only with no gather, so it suits fixed-point
  per-pixel maths -- the planned 3D noise patterns -- rather than the
  table-driven existing ones. Write a scalar version first, check the
  vector one against it in host tests, keep it only if `/api/v1/stats`
  shows a gain.

## Patterns

- Done: Clock redesigned (analog dial on top, digits on the right face, date
  on the left; push 24 ms -> 1.6 ms), Game of Life on all three faces across
  the seams, Snakes carry direction across seams, `/api/v1/patterns`.
- Done: Ripples (rings from the shared corner plus raindrops, 12 ms tick),
  Matrix Rain (over the edges and down the sides, 1 ms), Nebula (3D Perlin
  noise around the cube, 20 ms at quarter-resolution sampling).
- Done: Wireframes (solids floating in the isometric view, via
  cube::unproject), Rubik's Cube (scramble and solve, stickers placed in 3D),
  Falling Sand (poured from the top face, piling on the sides), Plane Sweep,
  Ticker (custom message, time and date across the side faces; message from
  the dashboard or /api/v1/ticker).
- Nebula is the SIMD/fixed-point candidate: its tick is noise evaluation.

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

- ESP-DASH Pro leaves `Widget::_index` uninitialized, so heap-allocated cards
  sort randomly (worked around with `setIndex()`). Fix in the ESP-DASH-Pro
  fork: initialize it to 0.

## Later

- Tag the TJpg_Decoder and spotify forks so the dependency updater can track
  them; consider `espressif/esp_jpeg` instead of the TJpg fork.
- Hold IDF 6 / Arduino 4 until HUB75-MatrixPanel-DMA supports it.
