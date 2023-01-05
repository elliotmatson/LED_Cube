#ifndef SPOTIFY_H
#define SPOTIFY_H

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "config.h"
#include "cube_utils.h"

#include <SpotifyArduino.h>
#include <TJpg_Decoder.h>

#include <WebServer.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

class Spotify : public Pattern
{
public:
  Spotify(MatrixPanel_I2S_DMA *display);
  void init();
  void show();
  ~Spotify();

private:
  void handleRoot();
  void handleCallback();
  void handleNotFound();
  void printCurrentlyPlayingToSerial(CurrentlyPlaying currentlyPlaying);
  bool displayOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
  int displayImage(char *albumArtUrl);

  MatrixPanel_I2S_DMA *display;
  WiFiClientSecure client;
  SpotifyArduino spotify;

  String lastAlbumArtUrl;

  char spotifyID[33];
  char spotifySecret[33];

  unsigned long delayBetweenRequests = 30000; // Time between requests (1 minute)
  unsigned long requestDueTime;               // time when request due
};

#endif