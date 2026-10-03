#ifndef SPOTIFY_H
#define SPOTIFY_H

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SpotifyArduino.h>
#include <TJpg_Decoder.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>

#include "config.h"
#include "cube_utils.h"
#include "spotify_sprites.h"
#include "LEMONMILK_Medium7pt7b.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#define BLANK_AFTER_PAUSE_MS 300000
#define SPOTIFY_REQUEST_INTERVAL_MS 1000
#define PROGRESS_REFRESH_MS 100

//#define SPOTIFY_RESET_OAUTH
//#define SPOTIFY_RESET_TOKEN

// get ESP-IDF Certificate Bundle
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

enum PatternStatus
{
  unknown, // before init(); lets the first changeStatus() always draw
  oauth,
  refreshToken,
  noPlayback,
  playback
};

class Spotify : public Pattern
{
public:
  Spotify();
  void init(PatternServices *pattern);
  void start();
  void stop();
  ~Spotify();

private:
  void refreshInfo();
  bool displayImageOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t *bitmap);
  int displayImage();
  void displayInfo();
  void displayProgress();
  void displayPlayback();
  void startOauthWebServer();
  void stopOauthWebServer();
  void exchangePendingCode();
  int setupCredentials();
  void changeStatus(PatternStatus status);

  SinglePanel *panel0 = nullptr;
  SinglePanel *panel1 = nullptr;
  SinglePanel *panel2 = nullptr;
  NetworkClientSecure client;
  SpotifyArduino *spotify = nullptr;
  Preferences spotifyPrefs;

  TaskHandle_t progressTask = nullptr;

  String previousTrack;
  String previousAlbum;
  long lastPlaying = 0;
  long lastUpdate = 0;

  CurrentlyPlaying currentlyPlaying{};
  PlayerDetails playerDetails{};
  PatternStatus patternStatus = unknown;
  std::vector<AsyncCallbackWebHandler *> handlers;

  // The OAuth callback runs in the AsyncTCP task, which must not block on the
  // token request's TLS handshake. It hands the code to the refresh task here.
  portMUX_TYPE codeMux = portMUX_INITIALIZER_UNLOCKED;
  char pendingCode[512] = "";
  // Random per /spotify visit and checked on /callback/, so a link from
  // elsewhere cannot log the cube in to someone else's account.
  char oauthState[17] = "";

  char spotifyID[33];
  char spotifySecret[33];
};

#endif