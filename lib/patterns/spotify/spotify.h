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

// get ESP-IDF Certificate Bundle
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");


class Spotify : public Pattern
{
public:
  Spotify(MatrixPanel_I2S_DMA *display, AsyncWebServer *server);
  void init();
  void show();
  ~Spotify();

private:
  void printCurrentlyPlayingToSerial(CurrentlyPlaying currentlyPlaying);
  bool displayOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
  int displayImage(char *albumArtUrl);

  MatrixPanel_I2S_DMA *display;
  WiFiClientSecure client;
  AsyncWebServer *server;
  SpotifyArduino spotify;
  Preferences spotifyPrefs;

  String lastAlbumArtUrl;

  char spotifyID[33];
  char spotifySecret[33];

  unsigned long delayBetweenRequests = 30000; // Time between requests (1 minute)
  unsigned long requestDueTime;               // time when request due
};

#endif