#include "cube.h"

// for signing FW on Github
const __attribute__((section(".rodata_custom_desc"))) CubePartition cubePartition = {CUBE_MAGIC_COOKIE};

// Create a new Cube object with optional devMode
Cube::Cube() : server(80),
               currentPattern(patterns::spotify),
               serial(String(ESP.getEfuseMac() % 0x1000000, HEX)),
               wifiReady(false),
               dashboard(&server),
               otaToggle(&dashboard, BUTTON_CARD, "OTA Update Enabled"),
               GHUpdateToggle(&dashboard, BUTTON_CARD, "Github Update Enabled"),
               developmentToggle(&dashboard, BUTTON_CARD, "Use Development Builds"),
               signedFWOnlyToggle(&dashboard, BUTTON_CARD, "Signed FW only"),
               fwVersion(&dashboard, "Firmware Version", FW_VERSION),
               brightnessSlider(&dashboard, SLIDER_CARD, "Brightness:", "", 0, 255),
               latchSlider(&dashboard, SLIDER_CARD, "Latch Blanking:", "", 1, 4),
               use20MHzToggle(&dashboard, BUTTON_CARD, "Use 20MHz Clock"),
               rebootButton(&dashboard, BUTTON_CARD, "Reboot Cube"),
               resetWifiButton(&dashboard, BUTTON_CARD, "Reset Wifi"),
               crashMe(&dashboard, BUTTON_CARD, "Crash Cube"),
               systemTab(&dashboard, "System"),
               displayTab(&dashboard, "Display"),
               developerTab(&dashboard, "Development")
{}

// initialize all cube tasks and functions
void Cube::init()
{
    Serial.begin(115200);
    initPrefs();
    initDisplay();
    initWifi();
    initUI();
    initUpdates();

    // Start the task to show the selected pattern
    /*xTaskCreate(
        [](void* o){ static_cast<Cube*>(o)->printMem(); },     // This is disgusting, but it works
        "Memory Printer",      // Name of the task (for debugging)
        2000,                // Stack size (bytes)
        this, // Parameter to pass
        1,                   // Task priority
        &printMemTask // Task handle
    );*/
    // Start the task to show the selected pattern
    xTaskCreate(
        [](void* o){ static_cast<Cube*>(o)->showPattern(); },     // This is disgusting, but it works
        "Show Pattern",      // Name of the task (for debugging)
        6000,                // Stack size (bytes)
        this, // Parameter to pass
        6,                   // Task priority
        &showPatternTask // Task handle
    );
}

// Initialize Preferences Library
void Cube::initPrefs()
{
    prefs.begin("cube");

    if ((!prefs.isKey("cubePrefs")) || (prefs.getBytesLength("cubePrefs") != sizeof(CubePrefs))) {
        this->cubePrefs.print("No valid preferences found, creating new");
        prefs.putBytes("cubePrefs", &cubePrefs, sizeof(CubePrefs));
    }
    prefs.getBytes("cubePrefs", &cubePrefs, sizeof(CubePrefs));
    this->cubePrefs.print("Loaded Preferences");
}

// Initialize update methods, setup check tasks
void Cube::initUpdates()
{
    this->setOTA(this->cubePrefs.ota);
    this->setGHUpdate(this->cubePrefs.github);
}

// Initialize display driver
void Cube::initDisplay()
{
    cubePrintf("Configuring HUB_75\n");
    HUB75_I2S_CFG::i2s_pins _pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
    HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER, _pins);
    if(cubePrefs.use20MHz) {
        mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;
    } else {
        mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
    }
    mxconfig.clkphase = false;
    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    setBrightness(this->cubePrefs.brightness);
    dma_display->setLatBlanking(cubePrefs.latchBlanking);

    // Allocate memory and start DMA display
    if (not dma_display->begin())
         cubePrintf("****** !KABOOM! I2S memory allocation failed ***********");
}

// Initialize wifi and prompt for connection if needed
void Cube::initWifi()
{
    pinMode(WIFI_LED, OUTPUT);
    digitalWrite(WIFI_LED, 0);
    cubePrintf("Connecting to WiFi...\n");
    wifiManager.setHostname("cube");
    wifiManager.setClass("invert");
    wifiManager.setAPCallback([&](WiFiManager *myWiFiManager)
        {
            dma_display->fillScreen(BLACK);
            dma_display->setTextColor(WHITE);
            dma_display->setCursor(0, 0);
            dma_display->printf("\n\nConnect to\n   WiFi\n\nSSID: %s", myWiFiManager->getConfigPortalSSID().c_str());
        });

    wifiManager.autoConnect("Cube");

    this->server.begin();
    WebSerial.begin(&this->server, "/log");

    this->wifiReady = true;

    cubePrintf("IP address: ");
    cubePrintf(WiFi.localIP().toString().c_str());
    digitalWrite(WIFI_LED, 1);
    MDNS.begin(HOSTNAME);
}

// initialize Cube UI Elements
void Cube::initUI()
{
    dashboard.setTitle("cube");

    this->otaToggle.attachCallback([&](int value)
        {
            this->setOTA(value);
            this->otaToggle.update(value);
            this->dashboard.sendUpdates(); 
        });
    this->developmentToggle.attachCallback([&](int value)
        {
            this->setDevelopment(value);
            this->developmentToggle.update(value);
            this->dashboard.sendUpdates(); 
        });
    this->GHUpdateToggle.attachCallback([&](int value)
        {
            this->setGHUpdate(value);
            this->GHUpdateToggle.update(value);
            this->dashboard.sendUpdates(); 
        });
    this->signedFWOnlyToggle.attachCallback([&](int value)
                                            {
            this->setSignedFWOnly(value);
            this->signedFWOnlyToggle.update(value);
            this->dashboard.sendUpdates(); });
    brightnessSlider.attachCallback([&](int value)
                                     {
            this->setBrightness(value);
            this->brightnessSlider.update(value);
            this->dashboard.sendUpdates(); });
    latchSlider.attachCallback([&](int value)
                                     {
            this->dma_display->setLatBlanking(value);
            this->cubePrefs.latchBlanking = value;
            this->updatePrefs();
            this->latchSlider.update(value);
            this->dashboard.sendUpdates(); });
    use20MHzToggle.attachCallback([&](int value)
                                  {
            this->cubePrefs.use20MHz = value;
            this->updatePrefs();
            this->use20MHzToggle.update(value);
            this->dashboard.sendUpdates(); });
    rebootButton.attachCallback([&](int value)
                                {
            cubePrintf("Rebooting...");
            ESP.restart();
            this->dashboard.sendUpdates(); });
    resetWifiButton.attachCallback([&](int value)
                                     {
            cubePrintf("Resetting WiFi...");
            wifiManager.resetSettings();
            ESP.restart();
            this->dashboard.sendUpdates(); });
    crashMe.attachCallback([&](int value)
                                     {
            cubePrintf("Crashing...");
            int *p = NULL;
            *p = 80;
            this->dashboard.sendUpdates(); });
    this->otaToggle.update(this->cubePrefs.ota);
    this->developmentToggle.update(this->cubePrefs.development);
    this->GHUpdateToggle.update(this->cubePrefs.github);
    this->brightnessSlider.update(this->cubePrefs.brightness);
    this->signedFWOnlyToggle.update(this->cubePrefs.signedFWOnly);
    this->latchSlider.update(this->cubePrefs.latchBlanking);
    this->use20MHzToggle.update(this->cubePrefs.use20MHz);
    this->rebootButton.update(true);
    this->resetWifiButton.update(true);

    this->otaToggle.setTab(&developerTab);
    this->developmentToggle.setTab(&developerTab);
    this->GHUpdateToggle.setTab(&developerTab);
    this->signedFWOnlyToggle.setTab(&developerTab);
    this->brightnessSlider.setTab(&displayTab);
    this->latchSlider.setTab(&displayTab);
    this->use20MHzToggle.setTab(&displayTab);
    this->rebootButton.setTab(&systemTab);
    this->resetWifiButton.setTab(&systemTab);

    dashboard.sendUpdates();
}

// set brightness of display
void Cube::setBrightness(uint8_t brightness)
{
    this->cubePrefs.brightness = brightness;
    this->updatePrefs();
    dma_display->setBrightness8(brightness);
}

// get brightness of display
uint8_t Cube::getBrightness()
{
    return this->cubePrefs.brightness;
}

// set OTA enabled/disabled
void Cube::setOTA(bool ota)
{
    cubePrefs.ota = ota;
    this->updatePrefs();
    if(ota) {
        cubePrintf("Starting OTA\n");
        ArduinoOTA.setHostname(HOSTNAME);
        ArduinoOTA
            .onStart([&]()
                     {
                    String type;
                    if (ArduinoOTA.getCommand() == U_FLASH)
                        type = "sketch";
                    else // U_SPIFFS
                        type = "filesystem";

                    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                    cubePrintf("Start updating %s", type);
                    vTaskDelete(showPatternTask);
                    dma_display->fillScreenRGB888(0, 0, 0);
                    dma_display->setCursor(6, 21);
                    dma_display->setTextColor(0xFFFF);
                    dma_display->setTextSize(3);
                    dma_display->print("OTA"); })
            .onEnd([&]()
                   {
                    cubePrintf("\nEnd\n"); 
                    for(int i = getBrightness(); i > 0; i=i-3) {
                        dma_display->setBrightness8(max(i, 0));
                    } })
            .onProgress([&](unsigned int progress, unsigned int total)
                        { 
                    this->dashboard.sendUpdates();
                    cubePrintf("Progress: %u%%\r", (progress / (total / 100)));

                    if (this->cubePrefs.signedFWOnly && progress == total)
                    {
                        CubePartition newCubePartition;
                        esp_partition_read(esp_ota_get_next_update_partition(NULL), sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t), &newCubePartition, sizeof(newCubePartition));
                        cubePrintf("Checking for Cube FW Signature: \nNew:%s\nold:%s\n", newCubePartition.cookie, cubePartition.cookie);
                        if (strcmp(newCubePartition.cookie, cubePartition.cookie))
                            Update.abort();
                    }

                    int i = map(progress, 0, total, 0, 512);
                    dma_display->drawFastHLine(128, 0, std::clamp(i, 0, 64), 0xFFFF);
                    dma_display->drawFastVLine(191, 0, std::clamp(i - 64, 0, 64), 0xFFFF);

                    dma_display->drawFastVLine(0, 64 - std::clamp(i - 128, 0, 63), std::clamp(i - 128, 0, 64), 0xFFFF);
                    dma_display->drawFastHLine(0, 0, std::clamp(i - 192, 0, 64), 0xFFFF);

                    dma_display->drawFastVLine(64, 64 - std::clamp(i - 256, 0, 63), std::clamp(i - 256, 0, 64), 0xFFFF);
                    dma_display->drawFastHLine(64, 0, std::clamp(i - 320, 0, 64), 0xFFFF);

                    dma_display->drawFastVLine(127, 0, std::clamp(i - 384, 0, 64), 0xFFFF);
                    dma_display->drawFastVLine(128, 0, std::clamp(i - 384, 0, 64), 0xFFFF);

                    dma_display->drawFastHLine(128 - std::clamp(i - 448, 0, 63), 63, std::clamp(i - 448, 0, 64), 0xFFFF);
                    dma_display->drawFastHLine(128, 63, std::clamp(i - 448, 0, 64), 0xFFFF);
                    dma_display->drawFastHLine(64 - std::clamp(i - 448, 0, 64), 63, std::clamp(i - 448, 0, 64), 0xFFFF);
                    dma_display->drawFastVLine(63, 64 - std::clamp(i - 448, 0, 64), std::clamp(i - 448, 0, 64), 0xFFFF); })
            .onError([&](ota_error_t error)
                     {
                    cubePrintf("Error[%u]: ", error);
                    if (error == OTA_AUTH_ERROR) cubePrintf("Auth Failed\n");
                    else if (error == OTA_BEGIN_ERROR) cubePrintf("Begin Failed\n");
                    else if (error == OTA_CONNECT_ERROR) cubePrintf("Connect Failed\n");
                    else if (error == OTA_RECEIVE_ERROR) cubePrintf("Receive Failed\n");
                    else if (error == OTA_END_ERROR) cubePrintf("End Failed\n"); });

        ArduinoOTA.begin();

        xTaskCreate(
            [](void* o){ static_cast<Cube*>(o)->checkForOTA(); }, // This is disgusting, but it works
            "Check For OTA", // Name of the task (for debugging)
            6000,            // Stack size (bytes)
            this,            // Parameter to pass
            5,               // Task priority
            &checkForOTATask // Task handle
        );
    } else
    {
        cubePrintf("OTA Disabled\n");
        ArduinoOTA.end();
        if(checkForOTATask){
            vTaskDelete(checkForOTATask);
        }
    }
}

// set Github Update enabled/disabled
void Cube::setGHUpdate(bool github)
{
    cubePrefs.github=github;
    this->updatePrefs();
    if(github) {
        cubePrintf("Github Update enabled...\n");
        httpUpdate.onStart([&]()
                           {
            cubePrintf("Start updating\n");
            vTaskDelete(showPatternTask);
            dma_display->fillScreenRGB888(0, 0, 0);
            dma_display->setCursor(6, 21);
            dma_display->setTextColor(0xFFFF);
            dma_display->setTextSize(3);
            dma_display->print("GHA"); });
        httpUpdate.onEnd([&]()
        { 
            cubePrintf("\nEnd\n"); 
            for(int i = getBrightness(); i > 0; i=i-3) {
                dma_display->setBrightness8(max(i, 0));
            }
        });
        httpUpdate.onProgress([&](unsigned int progress, unsigned int total)
                              { 
            cubePrintf("Progress: %u%%\r", (progress / (total / 100)));

            if (this->cubePrefs.signedFWOnly && progress == total)
            {
                CubePartition newCubePartition;
                esp_partition_read(esp_ota_get_next_update_partition(NULL), sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t), &newCubePartition, sizeof(newCubePartition));
                cubePrintf("Checking for Cube FW Signature: \nNew:%s\nold:%s\n", newCubePartition.cookie, cubePartition.cookie);
                if (strcmp(newCubePartition.cookie, cubePartition.cookie))
                    Update.abort();
            }

            int i = map(progress, 0, total, 0, 512);
            dma_display->drawFastHLine(128, 0, std::clamp(i, 0, 64), 0xFFFF);
            dma_display->drawFastVLine(191, 0, std::clamp(i - 64, 0, 64), 0xFFFF);

            dma_display->drawFastVLine(0, 64 - std::clamp(i - 128, 0, 63), std::clamp(i - 128, 0, 64), 0xFFFF);
            dma_display->drawFastHLine(0, 0, std::clamp(i - 192, 0, 64), 0xFFFF);

            dma_display->drawFastVLine(64, 64 - std::clamp(i - 256, 0, 63), std::clamp(i - 256, 0, 64), 0xFFFF);
            dma_display->drawFastHLine(64, 0, std::clamp(i - 320, 0, 64), 0xFFFF);

            dma_display->drawFastVLine(127, 0, std::clamp(i - 384, 0, 64), 0xFFFF);
            dma_display->drawFastVLine(128, 0, std::clamp(i - 384, 0, 64), 0xFFFF);

            dma_display->drawFastHLine(128 - std::clamp(i - 448, 0, 63), 63, std::clamp(i - 448, 0, 64), 0xFFFF);
            dma_display->drawFastHLine(128, 63, std::clamp(i - 448, 0, 64), 0xFFFF);
            dma_display->drawFastHLine(64 - std::clamp(i - 448, 0, 64), 63, std::clamp(i - 448, 0, 64), 0xFFFF);
            dma_display->drawFastVLine(63, 64 - std::clamp(i - 448, 0, 64), std::clamp(i - 448, 0, 64), 0xFFFF); });
        xTaskCreate(
            [](void* o){ static_cast<Cube*>(o)->checkForUpdates(); },     // This is disgusting, but it works
            "Check For Updates",    // Name of the task (for debugging)
            8000,                   // Stack size (bytes)
            this,                   // Parameter to pass
            5,                      // Task priority
            &checkForUpdatesTask    // Task handle
        );
    } else {
        cubePrintf("Github Updates Disabled\n");
        if(checkForUpdatesTask) {
            vTaskDelete(checkForUpdatesTask);
        }
    }
}

// set dev mode
void Cube::setDevelopment(bool development)
{ 
    cubePrefs.development = development;
    this->updatePrefs();
}

// set signedFWOnly
void Cube::setSignedFWOnly(bool signedFWOnly)
{ 
    cubePrefs.signedFWOnly = signedFWOnly;
    this->updatePrefs();
}

// update preferences stored in NVS
void Cube::updatePrefs()
{
    this->cubePrefs.print("Updating Preferences...");
    prefs.putBytes("cubePrefs", &cubePrefs, sizeof(CubePrefs));
}

// shows debug info on display
void Cube::showDebug()
{
    cubePrintf("Free Heap: %d / %d, Used PSRAM: %d / %d\n", ESP.getFreeHeap(), ESP.getHeapSize(), ESP.getPsramSize() - ESP.getFreePsram(), ESP.getPsramSize());
    cubePrintf("'%s' stack remaining: %d\n", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
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
        cubePrintf("Branch = %s\n", this->cubePrefs.development ? "development" : "master");
        if(this->cubePrefs.development) {
            // https://api.github.com/repos/elliotmatson/LED_Cube/releases
            String jsonUrl = String("https://api.github.com/repos/") + REPO_URL + String("/releases");
            cubePrintf("%s\n", jsonUrl.c_str());
            http.useHTTP10(true);
            if (http.begin(client, jsonUrl)) {
                SpiRamJsonDocument filter(200);
                filter[0]["name"] = true;
                filter[0]["prerelease"] = true;
                filter[0]["assets"] = true;
                filter[0]["published_at"] = true;
                http.GET();
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
                cubePrintf("Newest Prerelease: %s  date:%s\n", newestPrerelease["name"].as<String>().c_str(), newestPrerelease["published_at"].as<String>().c_str());
                // https://github.com/elliotmatson/LED_Cube/releases/download/v0.2.3/esp32.bin
                firmwareUrl = String("https://github.com/") + REPO_URL + String("/releases/download/") + newestPrerelease["name"].as<String>() + String("/esp32.bin");
                http.end();
            }
        } else {
            firmwareUrl = String("https://github.com/") + REPO_URL + String("/releases/latest/download/esp32.bin");
        }
        cubePrintf("%s\n", firmwareUrl.c_str());

        if (http.begin(client, firmwareUrl) && firmwareUrl != "") {
            int httpCode = http.sendRequest("HEAD");
            if (httpCode < 300 || httpCode > 400 || (http.getLocation().indexOf(String(FW_VERSION)) > 0) || (firmwareUrl.indexOf(String(FW_VERSION)) > 0))
            {
                cubePrintf("Not updating from (sc=%d): %s\n", httpCode, http.getLocation().c_str());
                http.end();
            }
            else
            {
                cubePrintf("Updating from (sc=%d): %s\n", httpCode, http.getLocation().c_str());

                httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
                t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

                switch (ret)
                {
                case HTTP_UPDATE_FAILED:
                    cubePrintf("Http Update Failed (Error=%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
                    break;

                case HTTP_UPDATE_NO_UPDATES:
                    cubePrintf("No Update!\n");
                    break;

                case HTTP_UPDATE_OK:
                    cubePrintf("Update OK!\n");
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
    for (;;) {
        Serial.printf("Free Heap: %d / %d, Used PSRAM: %d / %d\n",ESP.getFreeHeap(), ESP.getHeapSize(),ESP.getPsramSize() - ESP.getFreePsram(),ESP.getPsramSize());
        Serial.printf("'%s' stack remaining: %d\n", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
        char* buf = new char[2048];
        vTaskGetRunTimeStats(buf);
        Serial.println(buf);
        delete[] buf;
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void Cube::showPattern()
{
    switch (this->currentPattern) {
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
        case plasma:
        {
            Plasma plasma(this->dma_display);
            plasma.init();
            for (;;)
            {
                plasma.show();
                yield();
            }
            break;
        }
        case spotify:
        {
            Spotify spotify(this->dma_display, &server);
            spotify.init();
            for (;;)
            {
                spotify.show();
                yield();
            }
            break;
        }
    }
}