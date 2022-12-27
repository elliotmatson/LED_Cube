#ifndef SPOTIFY_H
#define SPOTIFY_H

#include <Arduino.h>
#include "config.h"
#include "cube.h"

#include <SpotifyArduino.h>
#include <SpotifyArduinoCert.h>
#include <TJpg_Decoder.h>

String lastAlbumArtUrl;
char scope[] = "user-read-playback-state%20user-modify-playback-state";

char callbackURI[] = "http%3A%2F%2Fcube.local%2Fcallback%2F";

WebServer server(80);

WiFiClientSecure client;
SpotifyArduino spotify(client);
char spotifyID[33];
char spotifySecret[33];

unsigned long delayBetweenRequests = 30000; // Time between requests (1 minute)
unsigned long requestDueTime;               // time when request due

const char *webpageTemplate =
    R"(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no" />
  </head>
  <body>
    <div>
     <a href="https://accounts.spotify.com/authorize?client_id=%s&response_type=code&redirect_uri=%s&scope=%s">spotify Auth</a>
    </div>
  </body>
</html>
)";

void handleRoot();
void handleCallback();
void handleNotFound();
void printCurrentlyPlayingToSerial(CurrentlyPlaying currentlyPlaying);
bool displayOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);
int displayImage(char *albumArtUrl);
void spotifyInit();
void spotifyLoop();

#endif