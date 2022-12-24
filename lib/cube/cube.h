#ifndef CUBE_H
#define CUBE_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoOTA.h>
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#include <FastLED.h>
#include <Preferences.h>
#include <WiFiManager.h>
//#include <WebSerial.h>

#include "config.h"

extern uint8_t const cos_wave[256];
extern TaskHandle_t checkForUpdatesTask;
extern TaskHandle_t checkForOTATask;
extern MatrixPanel_I2S_DMA *dma_display;
extern VirtualMatrixPanel *virtualDisp;
extern Preferences prefs;

void initPrefs();
void initUpdates();
void initDisplay();
void initWifi();
void showDebug();
inline uint8_t fastCosineCalc(uint16_t preWrapVal);
inline float projCalcX(uint8_t x, uint8_t y);
inline uint8_t projCalcIntX(uint8_t x, uint8_t y);
inline uint8_t projCalcY(uint8_t x, uint8_t y);
void firmwareUpdate();
void checkForUpdates(void *parameter);
void checkForOTA(void *parameter);
void printMem();

#endif