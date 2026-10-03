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
#include "spotify_sprites.h"
#include "LEMONMILK_Medium7pt7b.h"

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
  uint32_t frameInterval() const override { return 100; }

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

  static void setLastError(const std::string &error);
};

#endif
