#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Arduino.h>

#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "cube.h"

uint8_t const cos_wave[256] =
    {0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 4, 5, 6, 6, 8, 9, 10, 11, 12, 14, 15, 17, 18, 20, 22, 23, 25, 27, 29, 31, 33, 35, 38, 40, 42,
     45, 47, 49, 52, 54, 57, 60, 62, 65, 68, 71, 73, 76, 79, 82, 85, 88, 91, 94, 97, 100, 103, 106, 109, 113, 116, 119,
     122, 125, 128, 131, 135, 138, 141, 144, 147, 150, 153, 156, 159, 162, 165, 168, 171, 174, 177, 180, 183, 186,
     189, 191, 194, 197, 199, 202, 204, 207, 209, 212, 214, 216, 218, 221, 223, 225, 227, 229, 231, 232, 234, 236,
     238, 239, 241, 242, 243, 245, 246, 247, 248, 249, 250, 251, 252, 252, 253, 253, 254, 254, 255, 255, 255, 255,
     255, 255, 255, 255, 254, 254, 253, 253, 252, 252, 251, 250, 249, 248, 247, 246, 245, 243, 242, 241, 239, 238,
     236, 234, 232, 231, 229, 227, 225, 223, 221, 218, 216, 214, 212, 209, 207, 204, 202, 199, 197, 194, 191, 189,
     186, 183, 180, 177, 174, 171, 168, 165, 162, 159, 156, 153, 150, 147, 144, 141, 138, 135, 131, 128, 125, 122,
     119, 116, 113, 109, 106, 103, 100, 97, 94, 91, 88, 85, 82, 79, 76, 73, 71, 68, 65, 62, 60, 57, 54, 52, 49, 47, 45,
     42, 40, 38, 35, 33, 31, 29, 27, 25, 23, 22, 20, 18, 17, 15, 14, 12, 11, 10, 9, 8, 6, 6, 5, 4, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0};
TaskHandle_t checkForUpdatesTask = NULL;
TaskHandle_t checkForOTATask = NULL;
MatrixPanel_I2S_DMA *dma_display = nullptr;
Preferences prefs;

String serial;
String hostname;

// Initialize Preferences Library
void initPrefs()
{
    serial = String(ESP.getEfuseMac() % 0x1000000, HEX);
    hostname = "cube-" + serial;
    prefs.begin("cube");
}

// Initialize update methods, setup check tasks
void initUpdates()
{
#ifndef DEVELOPMENT
    Serial.printf("Github Update enabled...\n");
    xTaskCreate(
        checkForUpdates,     // Function that should be called
        "Check For Updates", // Name of the task (for debugging)
        6000,                // Stack size (bytes)
        NULL,                // Parameter to pass
        0,                   // Task priority
        &checkForUpdatesTask // Task handle
    );
#else
    Serial.printf("OTA Update Enabled\n");
    ArduinoOTA.setHostname("cube");
    ArduinoOTA
        .onStart([]()
                 {
                    String type;
                    if (ArduinoOTA.getCommand() == U_FLASH)
                        type = "sketch";
                    else // U_SPIFFS
                        type = "filesystem";

                    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                    Serial.println("Start updating " + type); 
                    dma_display->fillScreenRGB888(0, 0, 0); })
        .onEnd([]()
               { Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total)
                    { 
                        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
                        
                        int i = map(progress, 0, total, 0, 512);
                        if (i < 64) {
                            dma_display->drawPixelRGB888(128+i, 0, 255, 255, 255);
                        } else if (i < 128) {
                            dma_display->drawPixelRGB888(191, i-64, 255, 255, 255);
                        } else if (i < 192) {
                            dma_display->drawPixelRGB888(0, 63-(i-128), 255, 255, 255);
                        } else if (i < 256) {
                            dma_display->drawPixelRGB888((i-192), 0, 255, 255, 255);
                        } else if (i < 320) {
                            dma_display->drawPixelRGB888(64, 63 - (i - 256), 255, 255, 255);
                        } else if (i < 384) {
                            dma_display->drawPixelRGB888(64 + (i - 320), 0, 255, 255, 255);
                        } else if (i < 448) {
                            dma_display->drawPixelRGB888(127, (i - 384), 255, 255, 255);
                            dma_display->drawPixelRGB888(128, (i - 384), 255, 255, 255);
                        } else if (i < 512){
                            dma_display->drawPixelRGB888(127 - (i - 448), 63, 255, 255, 255);
                            dma_display->drawPixelRGB888(128 + (i - 448), 63, 255, 255, 255);
                            dma_display->drawPixelRGB888(63 - (i - 448), 63, 255, 255, 255);
                            dma_display->drawPixelRGB888(63, 63 - (i - 448), 255, 255, 255);
                        }
                    } )
        .onError([](ota_error_t error)
                 {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

    ArduinoOTA.begin();

    xTaskCreate(
        checkForOTA,     // Function that should be called
        "Check For OTA", // Name of the task (for debugging)
        6000,            // Stack size (bytes)
        NULL,            // Parameter to pass
        0,               // Task priority
        &checkForOTATask // Task handle
    );
#endif
}

// Initialize display driver
void initDisplay()
{
    Serial.printf("Configuring HUB_75\n");
    HUB75_I2S_CFG::i2s_pins _pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
    HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER, _pins);
    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
    mxconfig.clkphase = false;
    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    dma_display->setBrightness8(100);
    dma_display->setLatBlanking(2);

    // Allocate memory and start DMA display
    if (not dma_display->begin())
         Serial.printf("****** !KABOOM! I2S memory allocation failed ***********");
}

// Initialize wifi and prompt for connection if needed
void initWifi()
{
    pinMode(WIFI_LED, OUTPUT);
    digitalWrite(WIFI_LED, 0);
    Serial.printf("Connecting to WiFi...\n");
    WiFiManager wifiManager;
    wifiManager.setHostname("cube");
    wifiManager.setClass("invert");
    wifiManager.autoConnect("Cube");

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(WIFI_LED, 1);
}

// shows debug info on display
void showDebug()
{
    // prefs.putString("HW", "v0.0.1");
    // prefs.putString("SER", "005");
    dma_display->fillScreenRGB888(0, 0, 0);
    dma_display->setCursor(0, 0);
    dma_display->setTextColor(0xFFFF);
    dma_display->setTextSize(1);
    dma_display->printf("Wifi Conn\n%s\nHW: %s\nSW: %s\nSER: %s\n\nHostname:\n%s",
                        WiFi.localIP().toString().c_str(),
                        prefs.getString("HW").c_str(),
                        FW_VERSION,
                        serial.c_str(),
                        ArduinoOTA.getHostname().c_str());
    Serial.printf("Wifi Conn\n%s\nHW: %s\nSW: %s\nSER: %s",
                        WiFi.localIP().toString().c_str(),
                        prefs.getString("HW").c_str(),
                        FW_VERSION,
                        prefs.getString("SER").c_str());
}

void showCoordinates() {
    dma_display->fillScreenRGB888(0, 0, 0);
    dma_display->setTextColor(RED);
    dma_display->setTextSize(1);
    dma_display->drawFastHLine(0, 0, 192, 0x4208);
    dma_display->drawFastHLine(0, 63, 192, 0x4208);
    for(int i = 0; i < 3; i++) {
        int x0 = i*64;
        int y0 = 0;
        dma_display->drawFastVLine(i * 64, 0, 64, 0x4208);
        dma_display->drawFastVLine((i * 64) + 63, 0, 64, 0x4208);
        dma_display->drawPixel(x0, y0, RED);
        dma_display->drawPixel(x0, y0+63, GREEN);
        dma_display->drawPixel(x0+63, y0, BLUE);
        dma_display->drawPixel(x0+63, y0+63, YELLOW);
        dma_display->setTextColor(RED);
        dma_display->setCursor(x0 + 1, y0 + 1);
        dma_display->printf("%d,%d", x0, y0);
        dma_display->setTextColor(GREEN);
        dma_display->setCursor(x0+1, y0+55);
        dma_display->printf("%d,%d", x0, y0+63);
    }
    dma_display->setTextColor(BLUE);
    dma_display->setCursor(40, 1);
    dma_display->printf("%d,%d", 63, 0);
    dma_display->setCursor(98, 1);
    dma_display->printf("%d,%d", 127, 0);
    dma_display->setCursor(162, 1);
    dma_display->printf("%d,%d", 191, 0);
    dma_display->setTextColor(YELLOW);
    dma_display->setCursor(34, 55);
    dma_display->printf("%d,%d", 63, 63);
    dma_display->setCursor(92, 47);
    dma_display->printf("%d,%d", 127, 63);
    dma_display->setCursor(156, 47);
    dma_display->printf("%d,%d", 191, 63);
}

// Show a basic test sequence for testing panels
void showTestSequence()
{
  dma_display->fillScreenRGB888(255, 0, 0);
  delay(500);
  dma_display->fillScreenRGB888(0, 255, 0);
  delay(500);
  dma_display->fillScreenRGB888(0, 0, 255);
  delay(500);
  dma_display->fillScreenRGB888(255, 255, 255);
  delay(500);
  dma_display->fillScreenRGB888(0, 0, 0);

  for (uint8_t i = 0; i < 64 * PANELS_NUMBER; i++)
  {
    for (uint8_t j = 0; j < 64; j++)
    {
      dma_display->drawPixelRGB888(i, j, 255, 255, 255);
    }
    delay(50);
  }
  for (uint8_t i = 0; i < 64 * PANELS_NUMBER; i++)
  {
    for (uint8_t j = 0; j < 64; j++)
    {
      dma_display->drawPixelRGB888(i, j, 0, 0, 0);
    }
    delay(50);
  }
  for (uint8_t j = 0; j < 64; j++)
  {
    for (uint8_t i = 0; i < 64 * PANELS_NUMBER; i++)
    {
      dma_display->drawPixelRGB888(i, j, 255, 255, 255);
    }
    delay(50);
  }
  for (uint8_t j = 0; j < 64; j++)
  {
    for (uint8_t i = 0; i < 64 * PANELS_NUMBER; i++)
    {
      dma_display->drawPixelRGB888(i, j, 0, 0, 0);
    }
    delay(50);
  }
}

// Task to check for updates
#ifndef DEVELOPMENT
void checkForUpdates(void *parameter)
{
    for (;;)
    {
        firmwareUpdate();
        vTaskDelay((CHECK_FOR_UPDATES_INTERVAL * 1000) / portTICK_PERIOD_MS);
    }
}

// Check for web updates
void firmwareUpdate()
{
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    String firmwareUrl = String("https://github.com/") + REPO_URL + String("/releases/latest/download/esp32.bin");
    Serial.printf("%s\n",firmwareUrl);

    if (!http.begin(client, firmwareUrl))
        return;

    int httpCode = http.sendRequest("HEAD");
    if (httpCode < 300 || httpCode > 400 || http.getLocation().indexOf(String(FW_VERSION)) > 0)
    {
        Serial.printf("Not updating from (sc=%d): %s\n", httpCode, http.getLocation().c_str());
        http.end();
        return;
    }
    else
    {
        Serial.printf("Updating from (sc=%d): %s\n", httpCode, http.getLocation().c_str());
    }

    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
        Serial.printf("Http Update Failed (Error=%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
        break;

    case HTTP_UPDATE_NO_UPDATES:
        Serial.printf("No Update!\n");
        break;

    case HTTP_UPDATE_OK:
        Serial.printf("Update OK!\n");
        break;
    }
}
#else
void checkForOTA(void *parameter)
{
    for (;;)
    {
        ArduinoOTA.handle();
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}
#endif

void printMem() {
    Serial.printf("Free Heap: %d / %d, Used PSRAM: %d / %d\n",ESP.getFreeHeap(), ESP.getHeapSize(),ESP.getPsramSize() - ESP.getFreePsram(),ESP.getPsramSize());
    Serial.printf("'%s' stack remaining: %d\n", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
}