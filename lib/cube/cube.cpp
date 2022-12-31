#include "cube.h"


// Create a new Cube object with optional devMode
Cube::Cube()
{
    this->serial = String(ESP.getEfuseMac() % 0x1000000, HEX);
    this->server = new AsyncWebServer(80);
    this->dashboard = new ESPDash(this->server);
    this->otaToggle = new Card(this->dashboard, BUTTON_CARD, "OTA Update Enabled");
    this->GHUpdateToggle = new Card(this->dashboard, BUTTON_CARD, "Github Update Enabled");
    this->developmentToggle = new Card(this->dashboard, BUTTON_CARD, "Use Development Builds");
    this->fwVersion = new Statistic(this->dashboard, "Firmware Version", FW_VERSION);
    this->brightnessSlider = new Card(this->dashboard, SLIDER_CARD, "Brightness:", "", 0, 255);
    this->cubePrefs = new CubePrefs;
}

void Cube::init()
{
    Serial.begin(115200);
    initPrefs();
    initDisplay();
    initWifi();
    initUpdates();
    xTaskCreate(
        [](void* o){ static_cast<Cube*>(o)->showPattern(); },     // This is disgusting, but it works
        "Show Pattern",      // Name of the task (for debugging)
        6000,                // Stack size (bytes)
        this, // Parameter to pass
        3,                   // Task priority
        &showPatternTask // Task handle
    );
}

// Initialize Preferences Library
void Cube::initPrefs()
{
    prefs.begin("cube");

    if (!prefs.isKey("cubePrefs")) {
        Serial.printf("No preferences found, creating new\n");
        this->cubePrefs->brightness = 255;
        this->cubePrefs->development = false;
        this->cubePrefs->ota = false;
        this->cubePrefs->github = true;
        prefs.putBytes("cubePrefs", this->cubePrefs, sizeof(CubePrefs));
    }

    size_t schLen = prefs.getBytesLength("cubePrefs");
    if (schLen == sizeof(CubePrefs))
    {
        prefs.getBytes("cubePrefs", this->cubePrefs, schLen);
        Serial.printf("Loaded preferences\nBright: %d\nDev: %d\nOTA: %d\nGH: %d\n", cubePrefs->brightness, cubePrefs->development, cubePrefs->ota, cubePrefs->github);
    } else {
        log_e("Data is not correct size!");
    }
}

// Initialize update methods, setup check tasks
void Cube::initUpdates()
{
    if (this->cubePrefs->ota)
    {
        this->setOTA(true);
    }
    if (this->cubePrefs->github)
    {
        this->setGHUpdate(true);
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
    setBrightness(this->cubePrefs->brightness);
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

    this->server->begin();
    WebSerial.begin(this->server, "/log");
    this->otaToggle->attachCallback([&](bool value)
        {
            this->setOTA(value);
            this->otaToggle->update(value);
            this->dashboard->sendUpdates(); 
        });
    this->developmentToggle->attachCallback([&](bool value)
        {
            this->setDevelopment(value);
            this->developmentToggle->update(value);
            this->dashboard->sendUpdates(); 
        });
    this->GHUpdateToggle->attachCallback([&](bool value)
        {
            this->setGHUpdate(value);
            this->GHUpdateToggle->update(value);
            this->dashboard->sendUpdates(); 
        });
    brightnessSlider->attachCallback([&](int value)
                                     {
            this->setBrightness(value);
            this->brightnessSlider->update(value);
            this->dashboard->sendUpdates(); });
    this->otaToggle->update(this->cubePrefs->ota);
    this->developmentToggle->update(this->cubePrefs->development);
    this->GHUpdateToggle->update(this->cubePrefs->github);
    this->brightnessSlider->update(this->cubePrefs->brightness);
    dashboard->sendUpdates();
}

// set brightness of display
void Cube::setBrightness(uint8_t brightness)
{
    cubePrefs->brightness = brightness;
    this->updatePrefs();
    dma_display->setBrightness8(brightness);
}

// get brightness of display
uint8_t Cube::getBrightness()
{
    return this->cubePrefs->brightness;
}

// set OTA enabled/disabled
void Cube::setOTA(bool ota)
{
    cubePrefs->ota = ota;
    this->updatePrefs();
    if(ota) {
        Serial.printf("Starting OTA\n");
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
                    dma_display->fillScreenRGB888(0, 0, 0); })
            .onEnd([&]()
                   { 
                    Serial.println("\nEnd"); 
                    for(int i = getBrightness(); i > 0; i=i-3) {
                        dma_display->setBrightness8(max(i, 0));
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
            [](void* o){ static_cast<Cube*>(o)->checkForOTA(); }, // This is disgusting, but it works
            "Check For OTA", // Name of the task (for debugging)
            6000,            // Stack size (bytes)
            this,            // Parameter to pass
            0,               // Task priority
            &checkForOTATask // Task handle
        );
    } else {
        Serial.printf("Stopping OTA\n");
        vTaskDelete(checkForOTATask);
        ArduinoOTA.end();
    }
}

// set Github Update enabled/disabled
void Cube::setGHUpdate(bool github)
{
    cubePrefs->github=github;
    this->updatePrefs();
    if(github) {
        Serial.printf("Github Update enabled...\n");
        httpUpdate.onStart([&]()
        {
            Serial.println("Start updating");
            vTaskDelete(showPatternTask);
            setBrightness(100);
            dma_display->fillScreenRGB888(0, 0, 0);
        });
        httpUpdate.onEnd([&]()
        { 
            Serial.println("\nEnd"); 
            for(uint8_t i = getBrightness(); i > 0; i--) {
                setBrightness(i);
            }
        });
        httpUpdate.onProgress([&](unsigned int progress, unsigned int total)
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
            } 
        });
        xTaskCreate(
            [](void* o){ static_cast<Cube*>(o)->checkForUpdates(); },     // This is disgusting, but it works
            "Check For Updates",    // Name of the task (for debugging)
            8000,                   // Stack size (bytes)
            this,                   // Parameter to pass
            0,                      // Task priority
            &checkForUpdatesTask    // Task handle
        );
    } else {
        Serial.printf("Stopping Github Update\n");
        vTaskDelete(checkForUpdatesTask);
    }
}

// get dev mode
void Cube::setDevelopment(bool development)
{ 
    cubePrefs->development = development;
    this->updatePrefs();
}

// update preferences stored in NVS
void Cube::updatePrefs()
{
    Serial.printf("Updating preferences\nBright: %d\nDev: %d\nOTA: %d\nGH: %d\n", cubePrefs->brightness, cubePrefs->development, cubePrefs->ota, cubePrefs->github);
    prefs.putBytes("cubePrefs", cubePrefs, sizeof(CubePrefs));
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

// shows coordinates on display for debugging
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
void Cube::checkForUpdates()
{
    for (;;)
    {
        HTTPClient http;
        WiFiClientSecure client;
        client.setCACertBundle(rootca_crt_bundle_start);

        String firmwareUrl = "";
        Serial.printf("Branch = %s\n", this->cubePrefs->development ? "development" : "master");
        if(this->cubePrefs->development) {
            // https://api.github.com/repos/elliotmatson/LED_Cube/releases
            String jsonUrl = String("https://api.github.com/repos/") + REPO_URL + String("/releases");
            Serial.printf("%s\n", jsonUrl.c_str());
            http.useHTTP10(true);
            if (http.begin(client, jsonUrl)) {
                SpiRamJsonDocument filter(200);
                filter[0]["name"] = true;
                filter[0]["prerelease"] = true;
                filter[0]["assets"] = true;
                filter[0]["published_at"] = true;
                int httpCode = http.GET();
                SpiRamJsonDocument doc(4096);
                deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
                JsonArray releases = doc.as<JsonArray>();
                int newestPrereleaseIndex = -1;
                String newestPrereleaseDate = "";
                for (int i=0; i<releases.size(); i++)
                {
                    JsonObject release = releases[i].as<JsonObject>();
                    if (release["prerelease"].as<bool>() && release["published_at"].as<String>() > newestPrereleaseDate)
                    {
                        newestPrereleaseIndex = i;
                        newestPrereleaseDate = release["published_at"].as<String>();
                    }
                }
                JsonObject newestPrerelease = releases[newestPrereleaseIndex].as<JsonObject>();
                Serial.printf("Newest Prerelease: %s  date:%s\n", newestPrerelease["name"].as<String>().c_str(), newestPrerelease["published_at"].as<String>().c_str());
                // https://github.com/elliotmatson/LED_Cube/releases/download/v0.2.3/esp32.bin
                firmwareUrl = String("https://github.com/") + REPO_URL + String("/releases/download/") + newestPrerelease["name"].as<String>() + String("/esp32.bin");
                http.end();
            }
        } else {
            firmwareUrl = String("https://github.com/") + REPO_URL + String("/releases/latest/download/esp32.bin");
        }
        Serial.printf("%s\n", firmwareUrl.c_str());

        if (http.begin(client, firmwareUrl) && firmwareUrl != "") {
            int httpCode = http.sendRequest("HEAD");
            if (httpCode < 300 || httpCode > 400 || http.getLocation().indexOf(String(FW_VERSION)) > 0)
            {
                Serial.printf("Not updating from (sc=%d): %s\n", httpCode, http.getLocation().c_str());
                http.end();
            }
            else
            {
                Serial.printf("Updating from (sc=%d): %s\n", httpCode, http.getLocation().c_str());

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
        }
        vTaskDelay((CHECK_FOR_UPDATES_INTERVAL * 1000) / portTICK_PERIOD_MS);
    }
}

// Task to handle OTA updates
void Cube::checkForOTA()
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
    WebSerial.println("test Log");
}

void Cube::showPattern()
{
    patterns pattern = snake;
    switch (pattern) {
        case snake: {
            SnakeGame game(this->dma_display, 30, 3, 200);
            game.init();
            for (;;)
            {
                game.show();
                vTaskDelay(1 / portTICK_PERIOD_MS);
            }
            break;
        }
        case plasma: {
            Plasma plasma(this->dma_display);
            plasma.init();
            for (;;)
            {
                plasma.show();
                yield();
            }
            break;
        }
    }
}