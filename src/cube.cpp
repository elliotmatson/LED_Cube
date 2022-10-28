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
VirtualMatrixPanel *virtualDisp = nullptr;
Preferences prefs;

// Initialize update methods, setup check tasks
void initUpdates()
{
#ifdef VERSION
    Serial.printf("FW Version: " + VERSION);
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
        Serial.println("Start updating " + type); })
        .onEnd([]()
               { Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total)
                    { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
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
    dma_display->setBrightness8(255);
    dma_display->setLatBlanking(2);

    // Allocate memory and start DMA display
    if (not dma_display->begin())
        Serial.println("****** !KABOOM! I2S memory allocation failed ***********");
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
    prefs.begin("cube");
    // prefs.putString("HW", "v0.0.1");
    // prefs.putString("SER", "005");
    dma_display->fillScreenRGB888(0, 0, 0);
    dma_display->setCursor(1, 1);
    dma_display->setTextColor(0xFFFF);
    dma_display->setTextSize(1);
    dma_display->printf("Wifi Conn\n%s\nHW: %s\nSW: %s\nUP: %s\nSER: %s",
                        WiFi.localIP().toString(),
                        prefs.getString("HW"),
#ifndef VERSION
                        "dev",
                        "OTA",
#else
                        VERSION,
                        "Github",
#endif
                        prefs.getString("SER"));
}

// Calculates Cosine quickly using constants in flash
inline uint8_t fastCosineCalc(uint16_t preWrapVal)
{
    uint8_t wrapVal = (preWrapVal % 255);
    return (cos_wave[wrapVal]);
}

// Calculates precise projected X values of a pixel
inline float projCalcX(uint8_t x, uint8_t y)
{
    if (x < 64)
    { // Panel 1 (top)
        return 0.8660254 * (x - y);
    }
    else if (x < 128)
    { // Panel 2 (right)
        return 111.7173 - 0.8660254 * x;
    }
    else if (x < 192)
    { // Panel 3 (left)
        return 109.1192 - 0.8660254 * x;
    }
}

// Calculates less precise, but faster projected X values of a pixel
inline uint8_t projCalcIntX(uint8_t x, uint8_t y)
{
    if (x < 64)
    { // Panel 1 (top)
        return (7 * (x - y)) >> 3;
    }
    else if (x < 128)
    { // Panel 2 (right)
        return 112 - ((7 * x) >> 3);
    }
    else if (x < 192)
    { // Panel 3 (left)
        return 109 - ((7 * x) >> 3);
    }
}

// Calculates precise projected Y values of a pixel
inline uint8_t projCalcY(uint8_t x, uint8_t y)
{
    if (x < 64)
    { // Panel 1 (top)
        return (130 - x - y) >> 1;
    }
    else if (x < 128)
    { // Panel 2 (right)
        return y - (x >> 1);
    }
    else if (x < 192)
    { // Panel 3 (left)
        return (x >> 1) + y - 128;
    }
}

#ifdef VERSION
void checkForUpdates(void *parameter)
{
    for (;;)
    {
        firmwareUpdate();
        vTaskDelay((CHECK_FOR_UPDATES_INTERVAL * 1000) / portTICK_PERIOD_MS);
    }
}

void firmwareUpdate()
{
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    String firmwareUrl = String("https://github.com/") + REPO_URL + String("/releases/latest/download/esp32.bin");
    Serial.println(firmwareUrl);

    if (!http.begin(client, firmwareUrl))
        return;

    int httpCode = http.sendRequest("HEAD");
    if (httpCode < 300 || httpCode > 400 || http.getLocation().indexOf(String(VERSION)) > 0)
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