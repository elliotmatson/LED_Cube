#ifndef CONFIG_H
#define CONFIG_H

// SW Version (Github Actions automatically sets this)
#ifndef FW_VERSION
  #define FW_VERSION "DEV"
#endif

// Repo for automatic updates (Github Actions automatically sets this)
#ifndef REPO_URL
  #define REPO_URL "elliotmatson/LED_Cube"
#endif

// GitHub polling interval. Unauthenticated API calls are limited to 60 an hour
// per IP, shared with everything else on the network.
#define CHECK_FOR_UPDATES_INTERVAL 3600 // Seconds

// Signature for cube firmware
#define CUBE_MAGIC_COOKIE "LED_CUBE_FW"

// How long the WiFi setup hotspot stays up before the cube runs offline
#define WIFI_PORTAL_TIMEOUT 180 // Seconds

// Cube Hostname
#define HOSTNAME "cube"

// Panel Settings
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 64
#define PANELS_NUMBER 3

// NTP server
#define NTP_SERVER "pool.ntp.org"

// API Endpoint
#define API_ENDPOINT "/api"

// Where Spotify sends the browser after login. Spotify only accepts HTTPS here
// (http is allowed for 127.0.0.1 alone), which the cube cannot serve, so this is
// a relay page on GitHub Pages (docs/spotify/callback.html) that forwards to the
// cube. Must match a redirect URI registered in the Spotify app's dashboard.
#ifndef SPOTIFY_REDIRECT_URI
  #define SPOTIFY_REDIRECT_URI "https://elliotmatson.github.io/LED_Cube/spotify/callback.html"
#endif

// Where the dashboard's firmware upload card POSTs the image
#define FIRMWARE_UPLOAD_ROUTE "/firmware_upload"

// PlatformIO environment this firmware is built as. Release assets are named
// `<env>-<version>.bin`, so the GitHub updater looks for its image by this.
#ifndef FW_ENV
  #ifdef CONFIG_IDF_TARGET_ESP32S3
    #define FW_ENV "esp32-s3-devkitc-1"
  #else
    #define FW_ENV "esp32dev"
  #endif
#endif

// PCB pinouts
#ifdef CONFIG_IDF_TARGET_ESP32
  #define R1_PIN 4
  #define G1_PIN 15
  #define B1_PIN 5
  #define R2_PIN 19
  #define G2_PIN 18
  #define B2_PIN 22
  #define A_PIN 32
  #define B_PIN 23
  #define C_PIN 33
  #define D_PIN 14
  #define E_PIN 21
  #define LAT_PIN 27
  #define OE_PIN 26
  #define CLK_PIN 25
  #define USR_LED 2
  #define CONTROL_BUTTON 0
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define R1_PIN 11
  #define G1_PIN 12
  #define B1_PIN 13
  #define R2_PIN 14
  #define G2_PIN 21
  #define B2_PIN 47
  #define A_PIN 38
  #define B_PIN 39
  #define C_PIN 40
  #define D_PIN 41
  #define E_PIN 48
  #define LAT_PIN 2 //
  #define OE_PIN 1
  #define CLK_PIN 42
  #define USR_LED 10
  #define CONTROL_BUTTON 0
#endif

#endif