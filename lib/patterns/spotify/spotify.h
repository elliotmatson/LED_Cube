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

enum PatternStatus
{
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
  int setupCredentials();
  void changeStatus(PatternStatus status);

  SinglePanel *panel0;
  SinglePanel *panel1;
  SinglePanel *panel2;
  WiFiClientSecure client;
  SpotifyArduino *spotify;
  Preferences spotifyPrefs;

  TaskHandle_t progressTask;

  String previousTrack;
  String previousAlbum;
  long lastPlaying;
  long lastUpdate;

  CurrentlyPlaying currentlyPlaying;
  PlayerDetails playerDetails;
  PatternStatus patternStatus;
  std::vector<AsyncCallbackWebHandler *> handlers;

  char spotifyID[33];
  char spotifySecret[33];
};

#endif