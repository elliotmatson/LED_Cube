#include "cube.h"

Cube::Cube(bool devMode){
    this->devMode = devMode;
    this->serial = String(ESP.getEfuseMac() % 0x1000000, HEX);
}

void Cube::init()
{
    Serial.begin(115200);
    initPrefs();
    initDisplay();
    initWifi();
    initUpdates();
    if (this->getDevMode() == 0) {
        showDebug();
        delay(2000);
    }
    xTaskCreate(
        showPattern,         // Function that should be called
        "Show Pattern",      // Name of the task (for debugging)
        6000,                // Stack size (bytes)
        (void *) dma_display, // Parameter to pass
        3,                   // Task priority
        &showPatternTask // Task handle
    );
}

// Initialize Preferences Library
void Cube::initPrefs()
{
    prefs.begin("cube");
}

// Initialize update methods, setup check tasks
void Cube::initUpdates()
{
    if(this->getDevMode() == 0) {
        Serial.printf("Github Update enabled...\n");
        xTaskCreate(
            checkForUpdates,     // Function that should be called
            "Check For Updates", // Name of the task (for debugging)
            6000,                // Stack size (bytes)
            NULL,                // Parameter to pass
            0,                   // Task priority
            &checkForUpdatesTask // Task handle
        );
    } else {
        Serial.printf("OTA Update Enabled\n");
        ArduinoOTA.setHostname("cube");
        ArduinoOTA
            .onStart([&]()
                     {
                    String type;
                    if (ArduinoOTA.getCommand() == U_FLASH)
                        type = "sketch";
                    else // U_SPIFFS
                        type = "filesystem";

                    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                    Serial.println("Start updating " + type);
                    vTaskDelete(showPatternTask);
                    setBrightness(100);
                    dma_display->fillScreenRGB888(0, 0, 0); })
            .onEnd([&]()
                   { 
                    Serial.println("\nEnd"); 
                    for(uint8_t i = getBrightness(); i > 0; i--) {
                        setBrightness(i);
                    } })
            .onProgress([&](unsigned int progress, unsigned int total)
                        { 
                    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
                    
                    int i = map(progress, 0, total, 0, 512);
                    if (i < 64) {
                        dma_display->drawPixelRGB888(128+i, 0, 255, 255, 255);
                    } else if (i < 128) {
                        dma_display->drawPixelRGB888(191, i-64, 255, 255, 255);
                    }
                    else if (i < 192)
                    {
                        dma_display->drawPixelRGB888(0, 63-(i-128), 255, 255, 255);
                    } else if (i < 256) {
                        dma_display->drawPixelRGB888((i-192), 0, 255, 255, 255);
                    } else if (i < 320) {
                        dma_display->drawPixelRGB888(64, 63 - (i - 256), 255, 255, 255);
                    }
                    else if (i < 384)
                    {
                        dma_display->drawPixelRGB888(64 + (i - 320), 0, 255, 255, 255);
                    } else if (i < 448) {
                        dma_display->drawPixelRGB888(127, (i - 384), 255, 255, 255);
                        dma_display->drawPixelRGB888(128, (i - 384), 255, 255, 255);
                    } else if (i < 512){
                        dma_display->drawPixelRGB888(127 - (i - 448), 63, 255, 255, 255);
                        dma_display->drawPixelRGB888(128 + (i - 448), 63, 255, 255, 255);
                        dma_display->drawPixelRGB888(63 - (i - 448), 63, 255, 255, 255);
                        dma_display->drawPixelRGB888(63, 63 - (i - 448), 255, 255, 255);
                    } })
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
    }
}

// Initialize display driver
void Cube::initDisplay()
{
    Serial.printf("Configuring HUB_75\n");
    HUB75_I2S_CFG::i2s_pins _pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
    HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER, _pins);
    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
    mxconfig.clkphase = false;
    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    setBrightness(255);
    dma_display->setLatBlanking(2);

    // Allocate memory and start DMA display
    if (not dma_display->begin())
         Serial.printf("****** !KABOOM! I2S memory allocation failed ***********");
}

// Initialize wifi and prompt for connection if needed
void Cube::initWifi()
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

void Cube::setBrightness(uint8_t brightness)
{
    this->brightness = brightness;
    dma_display->setBrightness8(brightness);
}

uint8_t Cube::getBrightness()
{
    return this->brightness;
}

void Cube::setDevMode(bool devMode)
{
    this->devMode = devMode;
}

bool Cube::getDevMode()
{
    return this->devMode;
}

// shows debug info on display
void Cube::showDebug()
{
    Serial.printf("Free Heap: %d / %d, Used PSRAM: %d / %d\n", ESP.getFreeHeap(), ESP.getHeapSize(), ESP.getPsramSize() - ESP.getFreePsram(), ESP.getPsramSize());
    Serial.printf("'%s' stack remaining: %d\n", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
    // prefs.putString("HW", "v0.0.1");
    // prefs.putString("SER", "005");
    dma_display->fillScreenRGB888(0, 0, 0);
    dma_display->setCursor(0, 0);
    dma_display->setTextColor(0xFFFF);
    dma_display->setTextSize(1);
    dma_display->printf("%s\nH%s\nS%s\nSER: %s\nH: %d\nP: %d",
                        WiFi.localIP().toString().c_str(),
                        prefs.getString("HW").c_str(),
                        FW_VERSION,
                        serial.c_str(),
                        ESP.getFreeHeap(),
                        ESP.getFreePsram());
}

void Cube::showCoordinates() {
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
void Cube::showTestSequence()
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
void checkForUpdates(void *parameter)
{
    for (;;)
    {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();

        String firmwareUrl = String("https://github.com/") + REPO_URL + String("/releases/latest/download/esp32.bin");
        Serial.printf("%s\n", firmwareUrl.c_str());

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
        vTaskDelay((CHECK_FOR_UPDATES_INTERVAL * 1000) / portTICK_PERIOD_MS);
    }
}

void checkForOTA(void *parameter)
{
    for (;;)
    {
        ArduinoOTA.handle();
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}

void Cube::printMem()
{
    Serial.printf("Free Heap: %d / %d, Used PSRAM: %d / %d\n",ESP.getFreeHeap(), ESP.getHeapSize(),ESP.getPsramSize() - ESP.getFreePsram(),ESP.getPsramSize());
    Serial.printf("'%s' stack remaining: %d\n", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
}

void showPattern(void *parameter)
{
    MatrixPanel_I2S_DMA display = *(MatrixPanel_I2S_DMA *) parameter;
    patterns pattern = snake;
    switch (pattern) {
        case snake: {
            SnakeGame game(&display, 15, 3, 200);
            game.init();
            for (;;)
            {
                game.show();
            }
            break;
        }
        case plasma: {
            Plasma plasma(&display);
            plasma.init();
            for (;;)
            {
                plasma.show();
            }
            break;
        }
    }
}