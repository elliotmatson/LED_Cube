#ifndef CUBE_H
#define CUBE_H

#include "config.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
//#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#include <FastLED.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <Preferences.h>


extern uint8_t const cos_wave[256];
extern TaskHandle_t checkForUpdatesTask;
extern TaskHandle_t checkForOTATask;
extern MatrixPanel_I2S_DMA *dma_display;
extern VirtualMatrixPanel  *virtualDisp;
extern Preferences prefs;

void initUpdates();
void initDisplay();
void initWifi();
void showDebug();
inline uint8_t fastCosineCalc( uint16_t preWrapVal);
inline float projCalcX(uint8_t x, uint8_t y);
inline uint8_t projCalcIntX(uint8_t x, uint8_t y);
inline uint8_t projCalcY(uint8_t x, uint8_t y);
void firmwareUpdate();
void checkForUpdates(void * parameter);
void checkForOTA(void * parameter);



#endif