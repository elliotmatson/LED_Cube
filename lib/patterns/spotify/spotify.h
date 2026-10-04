#ifndef SPOTIFY_H
#define SPOTIFY_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SpotifyArduino.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <Preferences.h>
#include <functional>
#include <string>

#include "config.h"
#include "cube_utils.h"
#include "color.h"
#include "noise.h"
#include "spotify_sprites.h"
#include "fonts.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#define BLANK_AFTER_PAUSE_MS 300000
#define SPOTIFY_REQUEST_INTERVAL_MS 1000

//#define SPOTIFY_RESET_OAUTH
//#define SPOTIFY_RESET_TOKEN

// get ESP-IDF Certificate Bundle
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

enum PatternStatus
{
  unknown, // nothing drawn yet
  oauth,
  refreshToken,
  noPlayback,
  playback
};

/**
 * Now playing on Spotify: album art on face 2, track details, a progress bar
 * and playback state on face 1.
 *
 * Split in two, because every Spotify call is an HTTPS request of a second or
 * more:
 * - a worker task (begin() to end()) talks to Spotify and keeps a copy of
 *   what it learned, under stateMutex;
 * - tick(), in the render task, draws from that copy and never blocks on the
 *   network.
 * end() asks the worker to stop and waits for it, rather than deleting it
 * mid-request.
 */
class Spotify : public Pattern
{
public:
  Spotify();
  ~Spotify();
  void begin(PatternServices *services) override;
  void tick() override;
  void end() override;
  // Fast enough for smooth scrolling text; network work is in the worker.
  uint32_t frameInterval() const override { return 50; }

  // Account setup, usable whether or not the pattern is running. Stored in
  // NVS namespace "spotify". Changes take effect the next time the pattern
  // begins; Cube restarts it if it is showing.
  struct Account
  {
    std::string clientId;
    bool hasSecret = false;
    bool linked = false; // a refresh token is stored
  };
  static Account account();
  static void setClientId(const std::string &id);
  static void setClientSecret(const std::string &secret);
  static void logOut();
  /// The last login or token problem, for the dashboard; empty if none.
  static std::string lastError();

  /**
   * Registers the login routes, /spotify and /callback/, for the life of the
   * firmware -- not just while the pattern runs, so a login can start from
   * any pattern. `show` is called when Spotify hands back a login code: the
   * pattern's worker is what exchanges it, so it has to be running.
   */
  static void registerRoutes(AsyncWebServer &server, std::function<void()> show);

private:
  // What the worker learned about the current item. Owned copies: the
  // library's structs point into a JSON document freed when its callback
  // returns.
  struct NowPlaying
  {
    String trackUri;
    String albumUri;
    String trackName;
    String albumName;
    String artists;
    long progressMs = 0;
    long durationMs = 0;
    bool isPlaying = false;
    uint32_t receivedAtMs = 0;
  };
  struct PlayerState
  {
    bool isPlaying = false;
    bool shuffle = false;
    RepeatOptions repeat = repeat_off;
  };

  // Worker side
  void worker();
  int setupCredentials();
  void exchangePendingCode();
  void poll();
  void setStatus(PatternStatus s);

  // A line of text on face 1 that scrolls when it is wider than the face:
  // pause, scroll through, loop. Clears and redraws only its own band.
  struct Marquee
  {
    String text;
    const GFXfont *font = nullptr;
    int16_t x = 0;     // left margin
    int16_t y = 0;     // cursor (baseline for GFX fonts, top for the default)
    color::RGB color = {255, 255, 255};
    int16_t bandTop = 0, bandHeight = 0;
    int16_t width = 0;
    int16_t offset = 0;
    bool scrolls = false;
    uint32_t pauseUntil = 0;
  };
  void setMarquee(Marquee &m, const String &text, const GFXfont *font, int16_t x, int16_t y, int16_t bandTop, int16_t bandHeight, color::RGB color);
  void drawMarquee(Marquee &m);
  void stepMarquee(Marquee &m, uint32_t now);
  void drawAmbient(bool playing);
  void pickArtPalette();
  void startArtFade();
  void stepArtFade(uint32_t now);
  void stepText(const NowPlaying &np, bool newPlaying, uint32_t now);
  color::RGB paletteAt(int i, uint32_t now) const;

  // Render side
  void drawStatus(PatternStatus s);
  void drawInfo(const NowPlaying &np);
  void drawProgress(const NowPlaying &np);
  void drawPlayback(const PlayerState &pb);
  bool drawArtPixels(int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t *bitmap);

  SinglePanel *panel0 = nullptr; // album art, physical face 2 rotated 180
  SinglePanel *panel1 = nullptr; // details, physical face 1 rotated 180
  NetworkClientSecure client;
  SpotifyArduino *spotify = nullptr;
  Preferences spotifyPrefs;
  char spotifyID[33];
  char spotifySecret[33];
  uint32_t lastPlayingMs = 0;

  // Worker lifecycle
  TaskHandle_t workerTask = nullptr;
  SemaphoreHandle_t workerDone = nullptr;
  volatile bool running = false;

  // Shared between worker and tick(), under stateMutex. Versions count
  // changes, so tick() redraws only what changed.
  SemaphoreHandle_t stateMutex = nullptr;
  PatternStatus status = unknown;
  NowPlaying playing;
  uint32_t playingVersion = 0;
  PlayerState player;
  uint32_t playerVersion = 0;
  uint8_t *art = nullptr; // JPEG bytes, malloc'd by the library
  int artSize = 0;
  uint32_t artVersion = 0;

  // Render-side state
  PatternStatus drawnStatus = unknown;
  uint32_t drawnPlayingVersion = 0;
  uint32_t drawnPlayerVersion = 0;
  uint32_t drawnArtVersion = 0;
  uint8_t *shownArt = nullptr; // kept to redraw after a status change
  int shownArtSize = 0;
  Marquee lines[3]; // track, artists, album
  // Track changes fade the old text out, then the new text in.
  enum class TextFade : uint8_t
  {
    NONE,
    OUT,
    IN,
  };
  TextFade textFade = TextFade::NONE;
  uint32_t textFadeStartMs = 0;
  uint8_t textLevel = 255; // what drawMarquee() scales the text colour by
  bool textShown = false;  // something is on face 1 to fade out
  NowPlaying pendingInfo;
  // The cloud's brightness, gliding to full (playing) or dim (paused).
  uint16_t ambientLevel = 0;
  uint32_t ambientLevelMs = 0;
  bool shownPlaying = false;

  // Top face: a slow noise cloud in the album art's colours. Colours are
  // picked from a 4x4x4 histogram filled while the art decodes.
  struct Bucket
  {
    uint32_t count, r, g, b;
  };
  Bucket *histogram = nullptr;  // 64 entries, PSRAM
  float *ambient = nullptr;     // 32 x 32 noise samples, PSRAM
  // The cloud's colours glide from paletteFrom to artPalette over
  // PALETTE_FADE_MS after the album changes.
  color::RGB artPalette[3] = {{30, 215, 96}, {20, 90, 160}, {120, 40, 160}};
  color::RGB paletteFrom[3] = {{30, 215, 96}, {20, 90, 160}, {120, 40, 160}};
  uint32_t paletteFadeStartMs = 0;

  static const int16_t ART_SIZE = 64; // Spotify's smallest image
  static const int ART_BYTES = ART_SIZE * ART_SIZE * 3;
  // Album art cross-fades: artTarget is the decoded art, artShown what the
  // face shows; while fading, each tick draws a blend. 64 x 64 RGB888, PSRAM.
  uint8_t *artShown = nullptr;
  uint8_t *artTarget = nullptr;
  bool artFading = false;
  uint32_t artFadeStartMs = 0;
  uint32_t ambientStartMs = 0;
  uint32_t frame = 0;

  static void setLastError(const std::string &error);
};

#endif
