#ifndef CONFIG_H
#define CONFIG_H

// Github polling interval
#define CHECK_FOR_UPDATES_INTERVAL 5 // Seconds

// SW Version (Github Actions automatically sets this)
//#ifndef VERSION
//  #define VERSION "v0.0.6"
//#endif

// Repo for automatic updates (Github Actions automatically sets this)
#ifndef REPO_URL
  #define REPO_URL "elliotmatson/LED_Cube"
#endif

// Panel Settings
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 64
#define PANELS_NUMBER 3

// PCB pinouts
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

#define WIFI_LED 13
#define USR_LED 2


#endif