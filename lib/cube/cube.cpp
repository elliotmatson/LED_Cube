#include "cube.h"

// for signing FW on Github
const __attribute__((section(".rodata_custom_desc"))) CubePartition cubePartition = {CUBE_MAGIC_COOKIE};

// Create a new Cube object with optional devMode
Cube::Cube() : leds(4, USR_LED, NEO_GRB + NEO_KHZ800),
               server(80),
               serial(String(ESP.getEfuseMac() % 0x1000000, HEX)),
               wifiReady(false),
               dashboard(server),
               otaToggle(dashboard, "OTA Update Enabled"),
               GHUpdateToggle(dashboard, "Github Update Enabled"),
               developmentToggle(dashboard, "Use Development Builds"),
               signedFWOnlyToggle(dashboard, "Signed FW only"),
               fwVersion(dashboard, "Firmware Version"),
               brightnessSlider(dashboard, "Brightness:", 0, 255),
               latchSlider(dashboard, "Latch Blanking:", 1, 4),
               use20MHzToggle(dashboard, "Use 20MHz Clock"),
               rebootButton(dashboard, "Reboot Cube"),
               resetWifiButton(dashboard, "Reset Wifi"),
               crashMe(dashboard, "Crash Cube"),
               systemTab(dashboard, "System"),
               developerTab(dashboard, "Development")
{
    fwVersion.setValue(FW_VERSION);
}

// initialize all cube tasks and functions
void Cube::init()
{
    pinMode(CONTROL_BUTTON, INPUT_PULLUP);
    leds.begin();
    leds.setBrightness(20);
    leds.fill(leds.Color(255, 0, 0), 0, 0);
    leds.show(); // Initialize all pixels to 'off'
    Serial.begin(115200);
    if (initPrefs())
    {
        leds.setPixelColor(0, 0, 255, 0);
        leds.show();
    }
    if (initDisplay())
    {
        leds.setPixelColor(1, 0, 255, 0);
        leds.show();
    }
    if (initWifi())
    {
        leds.setPixelColor(2, 0, 255, 0);
        leds.show();
    }

    showDebug();
    delay(5000);

    // Set up pattern services
    patternServices.display = dma_display;
    patternServices.server = &server;

    initAPI();
    initUI();
    initUpdates();

    leds.setPixelColor(3, 0, 255, 0);
    leds.show();

    // make unordered map of patterns from the patterns list array
    // reserve capacity up front so push_back never reallocates: dash::Component only stores an
    // unowned const char*, so the backing std::string must never move for the life of the app
    patternButtonLabels.reserve(std::size(patternList));
    int i = 0;
    for (Pattern *pattern : patternList)
    {
        const std::string name = pattern->getName();
        patternButtonLabels.push_back(name + " Pattern");
        dash::PushButtonCard *card = new dash::PushButtonCard(dashboard, patternButtonLabels.back().c_str());
        ESPDash *dash = &dashboard;
        card->onPush([&, name, dash, i]()
                             {
                                ESP_LOGI("Cube", "Pattern: %s", name.c_str());
                                currentPattern->stop();
                                currentPattern = patterns[name];
                                currentPattern->init(&patternServices);
                                currentPattern->start();
                                this->cubePrefs.patternIndex = i;
                                this->updatePrefs();
                                dash->sendUpdates(); });
        patterns[pattern->getName()] = pattern;
        if (i == cubePrefs.patternIndex)
        {
            currentPattern = pattern;
        }
        i++;
    }
    dashboard.sendUpdates();

    // Start the task to show the selected pattern
    xTaskCreate(
        [](void *o)
        { static_cast<Cube *>(o)->printMem(); }, // This is disgusting, but it works
        "Memory Printer",                        // Name of the task (for debugging)
        3000,                                    // Stack size (bytes)
        this,                                    // Parameter to pass
        1,                                       // Task priority
        &printMemTask                            // Task handle
    );

    // Changes to a known "safe" pattern if the button is pressed
    if (digitalRead(CONTROL_BUTTON) == LOW)
    {
        currentPattern = patterns["Plasma"];
    }

    // Start the selected pattern
    ESP_LOGI("Cube", "currentPattern: %p", (void *)currentPattern);
    currentPattern->init(&patternServices);
    currentPattern->start();
}

// Initialize Preferences Library
bool Cube::initPrefs()
{
    bool status = prefs.begin("cube");

    if ((!prefs.isKey("cubePrefs")) || (prefs.getBytesLength("cubePrefs") != sizeof(CubePrefs)))
    {
        this->cubePrefs.print("No valid preferences found, creating new");
        prefs.putBytes("cubePrefs", &cubePrefs, sizeof(CubePrefs));
    }
    prefs.getBytes("cubePrefs", &cubePrefs, sizeof(CubePrefs));
    this->cubePrefs.print("Loaded Preferences");
    return status;
}

// Initialize update methods, setup check tasks
void Cube::initUpdates()
{
    this->setOTA(this->cubePrefs.ota);
    this->setGHUpdate(this->cubePrefs.github);
}

// Initialize display driver
bool Cube::initDisplay()
{
    bool status = false;
    ESP_LOGI(__func__, "Configuring HUB_75");
    HUB75_I2S_CFG::i2s_pins _pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
    HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER, _pins);
    if (cubePrefs.use20MHz)
    {
        mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;
    }
    else
    {
        mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
    }
    mxconfig.clkphase = false;
    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    dma_display->setLatBlanking(cubePrefs.latchBlanking);

    // Allocate memory and start DMA display
    if (dma_display->begin())
    {
        status = true;
    }
    else
    {
        ESP_LOGE(__func__, "****** !KABOOM! I2S memory allocation failed ***********");
    }
    setBrightness(this->cubePrefs.brightness);
    return status;
}

// Initialize wifi and prompt for connection if needed
bool Cube::initWifi()
{
    ESP_LOGI(__func__, "Connecting to WiFi...");
    wifiManager.setHostname("cube");
    wifiManager.setClass("invert");
    wifiManager.setAPCallback([&](WiFiManager *myWiFiManager)
                              {
            dma_display->fillScreen(BLACK);
            dma_display->setTextColor(WHITE);
            dma_display->setCursor(0, 0);
            dma_display->printf("\n\nConnect to\n   WiFi\n\nSSID: %s", myWiFiManager->getConfigPortalSSID().c_str());
            leds.setPixelColor(2, 0, 0, 255);
            leds.show(); });

    bool status = wifiManager.autoConnect("Cube");

    // Set up NTP
    long gmtOffset_sec = 0;
    int daylightOffset_sec = 0;

    // get GMT offset from public API
    WiFiClient client;
    HTTPClient http;
    http.begin(client, "http://worldtimeapi.org/api/ip");
    int httpCode = http.GET();
    if (httpCode > 0)
    {
        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();
            JsonDocument doc;
            deserializeJson(doc, payload);
            gmtOffset_sec = doc["raw_offset"].as<int>();
            daylightOffset_sec = doc["dst_offset"].as<int>();
        }
    }
    http.end();

    // Set time via NTP
    configTime(gmtOffset_sec, daylightOffset_sec, NTP_SERVER);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        ESP_LOGE(__func__, "Failed to obtain time");
    }
    ESP_LOGI(__func__, "Time set: %s", asctime(&timeinfo));

    // Set up web server
    this->server.begin();

    // Set up ElegantOTA (replaces removed PrettyOTA)
    ElegantOTA.begin(&this->server);
    xTaskCreate(
        [](void *o)
        { for (;;) { ElegantOTA.loop(); vTaskDelay(pdMS_TO_TICKS(1000)); } }, // This is disgusting, but it works
        "ElegantOTA Loop",                                                   // Name of the task (for debugging)
        3000,                                                                // Stack size (bytes)
        this,                                                                // Parameter to pass
        1,                                                                   // Task priority
        &elegantOtaTask                                                     // Task handle
    );

    this->wifiReady = true;

    ESP_LOGI(__func__, "IP address: ");
    ESP_LOGI(__func__, "%s", WiFi.localIP().toString().c_str());
    MDNS.begin(HOSTNAME);
    return status;
}

// initialize Cube UI Elements
void Cube::initUI()
{
    dashboard.setTitle("cube");

    this->otaToggle.onChange([&](bool state)
                                   {
            this->setOTA(state);
            this->otaToggle.setValue(state);
            this->dashboard.sendUpdates(); });
    this->developmentToggle.onChange([&](bool state)
                                           {
            this->setDevelopment(state);
            this->developmentToggle.setValue(state);
            this->dashboard.sendUpdates(); });
    this->GHUpdateToggle.onChange([&](bool state)
                                        {
            this->setGHUpdate(state);
            this->GHUpdateToggle.setValue(state);
            this->dashboard.sendUpdates(); });
    this->signedFWOnlyToggle.onChange([&](bool state)
                                            {
            this->setSignedFWOnly(state);
            this->signedFWOnlyToggle.setValue(state);
            this->dashboard.sendUpdates(); });
    brightnessSlider.onChange([&](int value)
                                    {
            this->setBrightness(value);
            this->brightnessSlider.setValue(value);
            this->dashboard.sendUpdates(); });
    latchSlider.onChange([&](int value)
                               {
            this->dma_display->setLatBlanking(value);
            this->cubePrefs.latchBlanking = value;
            this->updatePrefs();
            this->latchSlider.setValue(value);
            this->dashboard.sendUpdates(); });
    use20MHzToggle.onChange([&](bool state)
                                  {
            this->cubePrefs.use20MHz = state;
            this->updatePrefs();
            this->use20MHzToggle.setValue(state);
            this->dashboard.sendUpdates(); });
    rebootButton.onPush([&]()
                                {
            ESP_LOGI(__func__,"Rebooting...");
            ESP.restart();
            this->dashboard.sendUpdates(); });
    resetWifiButton.onPush([&]()
                                   {
            ESP_LOGI(__func__,"Resetting WiFi...");
            wifiManager.resetSettings();
            ESP.restart();
            this->dashboard.sendUpdates(); });
    crashMe.onPush([&]()
                           {
            ESP_LOGI(__func__,"Crashing...");
            int *p = NULL;
            *p = 80;
            this->dashboard.sendUpdates(); });
    this->otaToggle.setValue(this->cubePrefs.ota);
    this->developmentToggle.setValue(this->cubePrefs.development);
    this->GHUpdateToggle.setValue(this->cubePrefs.github);
    this->brightnessSlider.setValue(this->cubePrefs.brightness);
    this->signedFWOnlyToggle.setValue(this->cubePrefs.signedFWOnly);
    this->latchSlider.setValue(this->cubePrefs.latchBlanking);
    this->use20MHzToggle.setValue(this->cubePrefs.use20MHz);

    this->rebootButton.setTab(systemTab);
    this->resetWifiButton.setTab(systemTab);
    this->otaToggle.setTab(developerTab);
    this->developmentToggle.setTab(developerTab);
    this->GHUpdateToggle.setTab(developerTab);
    this->signedFWOnlyToggle.setTab(developerTab);
    this->crashMe.setTab(developerTab);
    this->latchSlider.setTab(developerTab);
    this->use20MHzToggle.setTab(developerTab);

    dashboard.sendUpdates();

    MDNS.addService("http", "tcp", 80);
}

/**
 * The function initializes the API and creates a JSON response containing information about patterns.
 */
void Cube::initAPI()
{
    char uri[128];

    // test endpoint
    sprintf(uri, "%s/v1/test", API_ENDPOINT);
    server.on(uri, HTTP_GET, [&](AsyncWebServerRequest *request)
              { request->send(200, "application/json", "{\"Hello\": \"world\"}"); });

    // get/set brightness in JSON
    sprintf(uri, "%s/v1/brightness", API_ENDPOINT);
    server.on(uri, HTTP_GET, [&](AsyncWebServerRequest *request)
              { request->send(200, "application/json", String("{\"brightness\":") + this->getBrightness() + "}"); });
    server.on(uri, HTTP_POST, [&](AsyncWebServerRequest *request)
              {
        //print request
        ESP_LOGI(__func__,"POST %s", request->url().c_str());
        if (request->hasArg("brightness"))
        {
            this->setBrightness(request->arg("brightness").toInt());
            request->send(200, "application/json", String("{\"brightness\":") + this->getBrightness() + "}");
        }
        else
        {
            request->send(400, "application/json", "{\"error\": \"No brightness parameter\"}");
        } });

    // redirect to docs on api root request
    server.on(API_ENDPOINT, HTTP_GET, [&](AsyncWebServerRequest *request)
              { request->redirect("https://github.com/elliotmatson/LED_Cube"); });
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
    if (ota)
    {
        ESP_LOGI(__func__, "Starting OTA");
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
                    ESP_LOGI(__func__,"Start updating %s", type.c_str());
                    currentPattern->stop();
                    dma_display->fillScreenRGB888(0, 0, 0);
                    dma_display->setFont(NULL);
                    dma_display->setCursor(6, 21);
                    dma_display->setTextColor(0xFFFF);
                    dma_display->setTextSize(3);
                    dma_display->print("OTA"); })
            .onEnd([&]()
                   {
                    ESP_LOGI(__func__,"End"); 
                    for(int i = getBrightness(); i > 0; i=i-3) {
                        dma_display->setBrightness8(max(i, 0));
                    } })
            .onProgress([&](unsigned int progress, unsigned int total)
                        { 
                    this->dashboard.sendUpdates();
                    ESP_LOGI(__func__,"Progress: %u%%\r", total ? (progress * 100) / total : 0);

                    if (this->cubePrefs.signedFWOnly && progress == total)
                    {
                        CubePartition newCubePartition;
                        esp_partition_read(esp_ota_get_next_update_partition(NULL), sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t), &newCubePartition, sizeof(newCubePartition));
                        ESP_LOGI(__func__,"Checking for Cube FW Signature: \nNew:%s\nold:%s", newCubePartition.cookie, cubePartition.cookie);
                        if (strcmp(newCubePartition.cookie, cubePartition.cookie))
                            Update.abort();
                    }

                    int i = map(progress, 0, total, 0, 512);
                    dma_display->drawFastHLine(128, 0, constrain(i, 0, 64), 0xFFFF);
                    dma_display->drawFastVLine(191, 0, constrain(i - 64, 0, 64), 0xFFFF);

                    dma_display->drawFastVLine(0, 64 - constrain(i - 128, 0, 63), constrain(i - 128, 0, 64), 0xFFFF);
                    dma_display->drawFastHLine(0, 0, constrain(i - 192, 0, 64), 0xFFFF);

                    dma_display->drawFastVLine(64, 64 - constrain(i - 256, 0, 63), constrain(i - 256, 0, 64), 0xFFFF);
                    dma_display->drawFastHLine(64, 0, constrain(i - 320, 0, 64), 0xFFFF);

                    dma_display->drawFastVLine(127, 0, constrain(i - 384, 0, 64), 0xFFFF);
                    dma_display->drawFastVLine(128, 0, constrain(i - 384, 0, 64), 0xFFFF);

                    dma_display->drawFastHLine(128 - constrain(i - 448, 0, 63), 63, constrain(i - 448, 0, 64), 0xFFFF);
                    dma_display->drawFastHLine(128, 63, constrain(i - 448, 0, 64), 0xFFFF);
                    dma_display->drawFastHLine(64 - constrain(i - 448, 0, 64), 63, constrain(i - 448, 0, 64), 0xFFFF);
                    dma_display->drawFastVLine(63, 64 - constrain(i - 448, 0, 64), constrain(i - 448, 0, 64), 0xFFFF); })
            .onError([&](ota_error_t error)
                     {
                    ESP_LOGE(__func__,"Error[%u]: ", error);
                    if (error == OTA_AUTH_ERROR) ESP_LOGE(__func__,"Auth Failed");
                    else if (error == OTA_BEGIN_ERROR) ESP_LOGE(__func__,"Begin Failed");
                    else if (error == OTA_CONNECT_ERROR) ESP_LOGE(__func__,"Connect Failed");
                    else if (error == OTA_RECEIVE_ERROR) ESP_LOGE(__func__,"Receive Failed");
                    else if (error == OTA_END_ERROR) ESP_LOGE(__func__,"End Failed"); });

        ArduinoOTA.begin();

        xTaskCreate(
            [](void *o)
            { static_cast<Cube *>(o)->checkForOTA(); }, // This is disgusting, but it works
            "Check For OTA",                            // Name of the task (for debugging)
            6000,                                       // Stack size (bytes)
            this,                                       // Parameter to pass
            5,                                          // Task priority
            &checkForOTATask                            // Task handle
        );
    }
    else
    {
        ESP_LOGI(__func__, "OTA Disabled");
        ArduinoOTA.end();
        if (checkForOTATask)
        {
            vTaskDelete(checkForOTATask);
        }
    }
}

/**
 * The function `setGHUpdate` enables or disables Github updates for a Cube object and performs
 * necessary actions based on the update status.
 *
 * @param github The parameter "github" is a boolean value that indicates whether GitHub updates are
 * enabled or disabled.
 */
void Cube::setGHUpdate(bool github)
{
    cubePrefs.github = github;
    this->updatePrefs();
    if (github)
    {
        ESP_LOGI(__func__, "Github Update enabled...");
        httpUpdate.onStart([&]()
                           {
            ESP_LOGI(__func__,"Start updating");
            currentPattern->stop();
            dma_display->fillScreenRGB888(0, 0, 0);
            dma_display->setFont(NULL);
            dma_display->setCursor(6, 21);
            dma_display->setTextColor(0xFFFF);
            dma_display->setTextSize(3);
            dma_display->print("GHA"); });
        httpUpdate.onEnd([&]()
                         { 
            ESP_LOGI(__func__,"End"); 
            for(int i = getBrightness(); i > 0; i=i-3) {
                dma_display->setBrightness8(max(i, 0));
            } });
        httpUpdate.onProgress([&](unsigned int progress, unsigned int total)
                              { 
            ESP_LOGI(__func__,"Progress: %u%%\r", total ? (progress * 100) / total : 0);

            if (this->cubePrefs.signedFWOnly && progress == total)
            {
                CubePartition newCubePartition;
                esp_partition_read(esp_ota_get_next_update_partition(NULL), sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t), &newCubePartition, sizeof(newCubePartition));
                ESP_LOGI(__func__,"Checking for Cube FW Signature: \nNew:%s\nold:%s", newCubePartition.cookie, cubePartition.cookie);
                if (strcmp(newCubePartition.cookie, cubePartition.cookie))
                    Update.abort();
            }

            int i = map(progress, 0, total, 0, 512);
            dma_display->drawFastHLine(128, 0, constrain(i, 0, 64), 0xFFFF);
            dma_display->drawFastVLine(191, 0, constrain(i - 64, 0, 64), 0xFFFF);

            dma_display->drawFastVLine(0, 64 - constrain(i - 128, 0, 63), constrain(i - 128, 0, 64), 0xFFFF);
            dma_display->drawFastHLine(0, 0, constrain(i - 192, 0, 64), 0xFFFF);

            dma_display->drawFastVLine(64, 64 - constrain(i - 256, 0, 63), constrain(i - 256, 0, 64), 0xFFFF);
            dma_display->drawFastHLine(64, 0, constrain(i - 320, 0, 64), 0xFFFF);

            dma_display->drawFastVLine(127, 0, constrain(i - 384, 0, 64), 0xFFFF);
            dma_display->drawFastVLine(128, 0, constrain(i - 384, 0, 64), 0xFFFF);

            dma_display->drawFastHLine(128 - constrain(i - 448, 0, 63), 63, constrain(i - 448, 0, 64), 0xFFFF);
            dma_display->drawFastHLine(128, 63, constrain(i - 448, 0, 64), 0xFFFF);
            dma_display->drawFastHLine(64 - constrain(i - 448, 0, 64), 63, constrain(i - 448, 0, 64), 0xFFFF);
            dma_display->drawFastVLine(63, 64 - constrain(i - 448, 0, 64), constrain(i - 448, 0, 64), 0xFFFF); });
        xTaskCreate(
            [](void *o)
            { static_cast<Cube *>(o)->checkForUpdates(); }, // This is disgusting, but it works
            "Check For Updates",                            // Name of the task (for debugging)
            8000,                                           // Stack size (bytes)
            this,                                           // Parameter to pass
            5,                                              // Task priority
            &checkForUpdatesTask                            // Task handle
        );
    }
    else
    {
        ESP_LOGI(__func__, "Github Updates Disabled");
        if (checkForUpdatesTask)
        {
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
void Cube::showCoordinates()
{
    dma_display->fillScreenRGB888(0, 0, 0);
    dma_display->setTextColor(RED);
    dma_display->setTextSize(1);
    dma_display->drawFastHLine(0, 0, 192, 0x4208);
    dma_display->drawFastHLine(0, 63, 192, 0x4208);
    for (int i = 0; i < 3; i++)
    {
        int x0 = i * 64;
        int y0 = 0;
        dma_display->drawFastVLine(i * 64, 0, 64, 0x4208);
        dma_display->drawFastVLine((i * 64) + 63, 0, 64, 0x4208);
        dma_display->drawPixel(x0, y0, RED);
        dma_display->drawPixel(x0, y0 + 63, GREEN);
        dma_display->drawPixel(x0 + 63, y0, BLUE);
        dma_display->drawPixel(x0 + 63, y0 + 63, YELLOW);
        dma_display->setTextColor(RED);
        dma_display->setCursor(x0 + 1, y0 + 1);
        dma_display->printf("%d,%d", x0, y0);
        dma_display->setTextColor(GREEN);
        dma_display->setCursor(x0 + 1, y0 + 55);
        dma_display->printf("%d,%d", x0, y0 + 63);
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
        String firmwareUrl;
        String tag;
        if (findFirmwareRelease(tag, firmwareUrl))
        {
            ESP_LOGI(__func__, "Updating %s -> %s from %s", FW_VERSION, tag.c_str(), firmwareUrl.c_str());
            NetworkClientSecure client;
            client.setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start);
            // browser_download_url redirects to the release asset CDN.
            httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
            t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

            switch (ret)
            {
            case HTTP_UPDATE_FAILED:
                ESP_LOGE(__func__, "Http Update Failed (Error=%d): %s", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
                break;

            case HTTP_UPDATE_NO_UPDATES:
                ESP_LOGI(__func__, "No Update!");
                break;

            case HTTP_UPDATE_OK:
                ESP_LOGI(__func__, "Update OK!");
                break;
            }
        }
        vTaskDelay((CHECK_FOR_UPDATES_INTERVAL * 1000) / portTICK_PERIOD_MS);
    }
}

/**
 * Finds the release the cube should be running and, if it is not the running
 * one, the URL of its application image.
 *
 * Releases are published by elliotmatson/pio-actions, which names the app
 * image `<env>-<tag>.bin`. That name carries the version, so the old
 * `/releases/latest/download/esp32s3.bin` shortcut cannot be used: the
 * release is looked up through the API and its asset matched by name.
 *
 * Stable cubes follow `/releases/latest`, which GitHub resolves to the newest
 * non-prerelease. Development cubes take the newest release of either kind.
 *
 * @param tag Set to the chosen release's tag.
 * @param firmwareUrl Set to the download URL of the image to install.
 * @return true only when there is an image to install. A tag that matches
 * FW_VERSION exactly, a local "DEV" build, or any failure along the way
 * returns false.
 */
bool Cube::findFirmwareRelease(String &tag, String &firmwareUrl)
{
    // A build that did not come from CI has no version to compare, and every
    // release would look newer than it. Overwriting it a minute after it was
    // flashed is never what was wanted.
    if (strcmp(FW_VERSION, "DEV") == 0)
    {
        ESP_LOGI(__func__, "Local build, not checking GitHub for updates");
        return false;
    }

    const bool development = this->cubePrefs.development;
    String apiUrl = String("https://api.github.com/repos/") + REPO_URL +
                    (development ? "/releases?per_page=10" : "/releases/latest");
    ESP_LOGI(__func__, "Checking %s", apiUrl.c_str());

    NetworkClientSecure client;
    client.setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start);
    HTTPClient http;
    http.useHTTP10(true); // no chunked encoding, so the body can be streamed into the parser
    if (!http.begin(client, apiUrl))
    {
        ESP_LOGE(__func__, "Could not start request");
        return false;
    }
    http.addHeader("Accept", "application/vnd.github+json");
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        // 404 from /releases/latest just means nothing has been published yet.
        ESP_LOGW(__func__, "GitHub returned %d", httpCode);
        http.end();
        return false;
    }

    // Only what is used below; a release's JSON is several KB per asset otherwise.
    JsonDocument filter(spiRamAllocator());
    JsonObject releaseFilter = development ? filter[0].to<JsonObject>() : filter.to<JsonObject>();
    releaseFilter["tag_name"] = true;
    releaseFilter["draft"] = true;
    releaseFilter["published_at"] = true;
    releaseFilter["assets"][0]["name"] = true;
    releaseFilter["assets"][0]["browser_download_url"] = true;

    JsonDocument doc(spiRamAllocator());
    DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (err)
    {
        ESP_LOGE(__func__, "Could not parse release list: %s", err.c_str());
        return false;
    }

    JsonObject release;
    if (development)
    {
        // ISO 8601 timestamps in the same zone compare correctly as strings.
        const char *newest = "";
        for (JsonObject candidate : doc.as<JsonArray>())
        {
            const char *published = candidate["published_at"] | "";
            if (!(candidate["draft"] | false) && strcmp(published, newest) > 0)
            {
                release = candidate;
                newest = published;
            }
        }
    }
    else
    {
        release = doc.as<JsonObject>();
    }
    if (release.isNull() || !release["tag_name"].is<const char *>())
    {
        ESP_LOGW(__func__, "No release found");
        return false;
    }

    tag = release["tag_name"].as<const char *>();
    // Exact match: a substring test would treat v0.2.1 as already running v0.2.10.
    if (tag == FW_VERSION)
    {
        ESP_LOGI(__func__, "Already running %s", FW_VERSION);
        return false;
    }

    String assetName = String(FW_ENV) + "-" + tag + ".bin";
    for (JsonObject asset : release["assets"].as<JsonArray>())
    {
        if (assetName == (asset["name"] | ""))
        {
            firmwareUrl = asset["browser_download_url"].as<const char *>();
            return firmwareUrl.length() > 0;
        }
    }
    ESP_LOGW(__func__, "Release %s has no %s", tag.c_str(), assetName.c_str());
    return false;
}

// Task to handle OTA updates
void Cube::checkForOTA()
{
    for (;;)
    {
        ArduinoOTA.handle();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void Cube::printMem()
{
    for (;;)
    {
        ESP_LOGI(__func__, "Free Heap: %d / %d, Used PSRAM: %d / %d", ESP.getFreeHeap(), ESP.getHeapSize(), heap_caps_get_total_size(MALLOC_CAP_SPIRAM) - heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        ESP_LOGI(__func__, "Largest free block in Heap: %d, PSRAM: %d", ESP.getMaxAllocHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        /*char *buf = new char[2048];
        vTaskGetRunTimeStats(buf);
        Serial.println(buf);
        delete[] buf;
        buf = new char[2048];
        vTaskList(buf);
        Serial.println(buf);
        delete[] buf;*/
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
