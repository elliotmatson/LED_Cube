#include "cube.h"


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
               timezoneDropdown(dashboard, "Time Zone", timezones::dropdownOptions()),
               tickerInput(dashboard, "Ticker Message", "Shown by the Ticker pattern"),
               spotifyStatus(dashboard, "Spotify", dash::Status::NONE),
               spotifyLogin(dashboard, "Log in to Spotify"),
               spotifyClientId(dashboard, "Spotify Client ID", "From developer.spotify.com"),
               spotifyClientSecret(dashboard, "Spotify Client Secret", "Saved; type to replace"),
               spotifyLogout(dashboard, "Log out of Spotify"),
               bootStatus(dashboard, "Startup", dash::Status::NONE),
               weatherStatus(dashboard, "Weather", dash::Status::NONE),
               weatherLocation(dashboard, "Weather Location", "City or postcode; blank to use the cube's IP address"),
               weatherMetric(dashboard, "Weather in \u00b0C and km/h"),
               updates(dashboard),
               systemTab(dashboard, "System"),
               developerTab(dashboard, "Development"),
               spotifyTab(dashboard, "Spotify"),
               weatherTab(dashboard, "Weather")
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
    // First, so a startup that resets is on record (see BootLog).
    bootLog.begin();
    if (settings.begin())
    {
        leds.setPixelColor(0, 0, 255, 0);
        leds.show();
    }
    bootLog.reached(BootLog::BOOT_SETTINGS);
    if (initDisplay())
    {
        leds.setPixelColor(1, 0, 255, 0);
        leds.show();
    }
    bootLog.reached(BootLog::BOOT_DISPLAY);
    if (initWifi())
    {
        leds.setPixelColor(2, 0, 255, 0);
        leds.show();
    }
    bootLog.reached(BootLog::BOOT_WIFI);

    showDebug();
    delay(5000);

    renderer.begin(dma_display, &server, [this](Pattern *pattern)
                   {
        settings.setPattern(pattern->getId());
        dashboard.sendUpdates(); });
    bootLog.reached(BootLog::BOOT_PATTERNS);

    initAPI();
    bootLog.reached(BootLog::BOOT_API);
    initUI();
    bootLog.reached(BootLog::BOOT_UI);
    updates.begin(server, dma_display, settings, renderer, systemTab);
    bootLog.reached(BootLog::BOOT_UPDATES);

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
        card->onPush([this, name, i]()
                     {
                        ESP_LOGI("Cube", "Pattern requested: %s", name.c_str());
                        this->renderer.requestPattern(i); });
        // ESP-DASH sorts cards by index, and a Widget's index is not
        // initialized: cards allocated here got heap garbage and came up in a
        // different order every boot. After the brightness slider (index 0).
        card->setIndex(10 + i);
        patterns[pattern->getName()] = pattern;
        i++;
    }
    // Saved by id, so adding or reordering patterns does not change which one
    // a cube comes back to. Firmware before that saved a position.
    std::string savedPattern = settings.pattern();
    int legacyIndex = settings.legacyPatternIndex();
    if (savedPattern.empty() && legacyIndex >= 0 && size_t(legacyIndex) < std::size(patternList))
    {
        savedPattern = patternList[legacyIndex]->getId();
    }
    size_t startIndex = 0;
    for (size_t p = 0; p < std::size(patternList); p++)
    {
        if (patternList[p]->getId() == savedPattern)
        {
            startIndex = p;
        }
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
        for (size_t p = 0; p < std::size(patternList); p++)
        {
            if (patternList[p] == patterns["Plasma"])
            {
                startIndex = p;
            }
        }
    }

    renderer.requestPattern(startIndex);

    // Startup got this far, so the image works: stop the bootloader rolling
    // it back (see verifyRollbackLater() in main.cpp). A no-op unless this is
    // the first boot after an update.
    esp_ota_mark_app_valid_cancel_rollback();
    bootLog.reached(BootLog::BOOT_CONFIRMED);
}

/**
 * Switches to the pattern with this id. With onlyIfShowing, restarts it only
 * if it is already the current one (to pick up changed settings).
 */
void Cube::showPattern(const char *id, bool onlyIfShowing)
{
    if (onlyIfShowing && settings.pattern() != id)
    {
        return;
    }
    for (size_t i = 0; i < std::size(patternList); i++)
    {
        if (patternList[i]->getId() == id)
        {
            renderer.requestPattern(i);
            return;
        }
    }
}

void Cube::refreshSpotifyStatus()
{
    const Spotify::Account a = Spotify::account();
    const std::string error = Spotify::lastError();
    if (a.clientId.empty() || !a.hasSecret)
    {
        spotifyStatus.setFeedback("Enter your Spotify app's Client ID and Secret below", dash::Status::WARNING);
    }
    else if (!error.empty())
    {
        spotifyStatus.setFeedback(error.c_str(), dash::Status::DANGER);
    }
    else if (!a.linked)
    {
        spotifyStatus.setFeedback("Not logged in - use Log in to Spotify", dash::Status::INFO);
    }
    else
    {
        spotifyStatus.setFeedback("Logged in", dash::Status::SUCCESS);
    }
}

void Cube::refreshWeatherStatus()
{
    const WeatherPattern::Report r = WeatherPattern::report();
    if (r.error[0] && !r.current)
    {
        weatherStatus.setFeedback(r.error, dash::Status::DANGER);
    }
    else if (r.current)
    {
        char text[128];
        snprintf(text, sizeof(text), "%s%s: %d\u00b0%s, %s", r.place, r.fromIp ? " (from the cube's IP address)" : "",
                 weather::temperature(r.tempC, r.metric), r.metric ? "C" : "F", weather::describe(r.code));
        weatherStatus.setFeedback(text, dash::Status::SUCCESS);
    }
    else if (r.located)
    {
        weatherStatus.setFeedback((std::string(r.place) + ": fetching the forecast").c_str(), dash::Status::INFO);
    }
    else
    {
        weatherStatus.setFeedback("Looked up when the Weather pattern runs", dash::Status::INFO);
    }
}

/// From the dashboard or the API: save, pass on, and show the result.
void Cube::setWeatherLocation(std::string text)
{
    while (!text.empty() && isspace((unsigned char)text.back()))
        text.pop_back();
    while (!text.empty() && isspace((unsigned char)text.front()))
        text.erase(text.begin());
    if (text.size() > 60)
    {
        text.resize(60);
    }
    settings.setWeatherLocation(text);
    WeatherPattern::setLocation(text);
    weatherLocation.setValue(text.c_str());
    // Show it: the pattern looks the place up, and the status follows.
    showPattern("weather");
    refreshWeatherStatus();
    dashboard.sendUpdates();
}

/// The saved time zone, or the default if none is saved or it is unknown.
const timezones::Zone &Cube::currentTimezone()
{
    const timezones::Zone *zone = timezones::find(settings.timezone().c_str());
    return zone ? *zone : timezones::DEFAULT_ZONE;
}

// Initialize display driver
bool Cube::initDisplay()
{
    bool status = false;
    ESP_LOGI(__func__, "Configuring HUB_75");
    HUB75_I2S_CFG::i2s_pins _pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
    HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER, _pins);
    if (settings.use20MHz())
    {
        mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;
    }
    else
    {
        mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
    }
    mxconfig.clkphase = false;
    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    dma_display->setLatBlanking(settings.latchBlanking());

    // Allocate memory and start DMA display
    if (dma_display->begin())
    {
        status = true;
    }
    else
    {
        ESP_LOGE(__func__, "****** !KABOOM! I2S memory allocation failed ***********");
    }
    setBrightness(settings.brightness());
    return status;
}

// Initialize wifi and prompt for connection if needed
bool Cube::initWifi()
{
    ESP_LOGI(__func__, "Connecting to WiFi...");

    // The setup hotspot gets a fresh password each boot, shown only on the
    // panels: joining it then needs someone who can see the cube, rather than
    // anyone in range of an open "Cube" network. No 0/O/1/l/I to misread.
    static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
    char apPassword[9];
    for (int i = 0; i < 8; i++)
    {
        apPassword[i] = alphabet[esp_random() % (sizeof(alphabet) - 1)];
    }
    apPassword[8] = '\0';

    wifiManager.setHostname("cube");
    wifiManager.setClass("invert");
    // Non-blocking, so the portal is driven by the loop below. WiFiManager's
    // own blocking loop only calls yield(), which never lets the idle task on
    // this core run, and the task watchdog fires for as long as it is up.
    wifiManager.setConfigPortalBlocking(false);
    wifiManager.setAPCallback([this, apPassword](WiFiManager *myWiFiManager)
                              {
            dma_display->fillScreen(BLACK);
            dma_display->setTextColor(WHITE);
            dma_display->setCursor(0, 0);
            dma_display->printf("\n\nConnect to\n   WiFi\n\nSSID: %s\nPassword:\n  %s", myWiFiManager->getConfigPortalSSID().c_str(), apPassword);
            leds.setPixelColor(2, 0, 0, 255);
            leds.show(); });

    // Modem sleep (the Arduino default) dozes between beacons, so packets wait
    // for the next DTIM wake: pings of 1-3 s and a sluggish dashboard, though
    // bulk transfers stay fast. The cube is on mains power; stay awake.
    WiFi.setSleep(false);
    // Try the saved network a few times before falling back to the setup
    // portal: one failed attempt (the router slow to answer after the cube
    // resets) otherwise put the cube in setup mode for three minutes.
    wifiManager.setConnectRetries(3);
    bool status = wifiManager.autoConnect("Cube", apPassword);
    if (!status)
    {
        // The portal is up. Give up after a while and run patterns offline,
        // rather than leaving the hotspot up indefinitely.
        const uint32_t portalStart = millis();
        while (!(status = wifiManager.process()))
        {
            if (millis() - portalStart > WIFI_PORTAL_TIMEOUT * 1000UL)
            {
                ESP_LOGW(__func__, "No WiFi after %d s of setup portal, continuing offline", WIFI_PORTAL_TIMEOUT);
                wifiManager.stopConfigPortal();
                dma_display->fillScreen(BLACK);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    // Time zone from the dashboard setting; the clock itself from NTP, in
    // the background. This used to ask worldtimeapi.org for the offset over
    // plain HTTP at boot, which failed often (leaving the cube on UTC) and
    // never followed a daylight-saving change until the next reboot -- a
    // POSIX TZ rule does both. Nothing waits for the first sync: patterns
    // that show the time check getLocalTime() themselves.
    configTzTime(currentTimezone().posix, NTP_SERVER);

    // Set up web server
    this->server.begin();

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
            updates.setOta(state);
            this->otaToggle.setValue(state);
            this->dashboard.sendUpdates(); });
    this->developmentToggle.onChange([&](bool state)
                                           {
            this->setDevelopment(state);
            this->developmentToggle.setValue(state);
            this->dashboard.sendUpdates(); });
    this->GHUpdateToggle.onChange([&](bool state)
                                        {
            updates.setGithub(state);
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
            settings.setLatchBlanking(value);
            this->latchSlider.setValue(value);
            this->dashboard.sendUpdates(); });
    use20MHzToggle.onChange([&](bool state)
                                  {
            settings.setUse20MHz(state);
            this->use20MHzToggle.setValue(state);
            this->dashboard.sendUpdates(); });
    rebootButton.onPush([&]()
                                {
            ESP_LOGI(__func__,"Rebooting...");
            settings.flush();
            ESP.restart();
            this->dashboard.sendUpdates(); });
    resetWifiButton.onPush([&]()
                                   {
            ESP_LOGI(__func__,"Resetting WiFi...");
            wifiManager.resetSettings();
            settings.flush();
            ESP.restart();
            this->dashboard.sendUpdates(); });
    crashMe.onPush([&]()
                           {
            ESP_LOGI(__func__,"Crashing...");
            int *p = NULL;
            *p = 80;
            this->dashboard.sendUpdates(); });
    timezoneDropdown.onChange([this](const dash::string &name)
                              {
            const timezones::Zone *zone = timezones::find(name.c_str());
            if (zone == nullptr)
            {
                return;
            }
            settings.setTimezone(zone->name);
            setenv("TZ", zone->posix, 1);
            tzset();
            timezoneDropdown.setValue(zone->name);
            dashboard.sendUpdates(); });
    this->otaToggle.setValue(settings.ota());
    this->developmentToggle.setValue(settings.development());
    this->GHUpdateToggle.setValue(settings.github());
    this->brightnessSlider.setValue(settings.brightness());
    this->signedFWOnlyToggle.setValue(settings.signedFirmwareOnly());
    this->latchSlider.setValue(settings.latchBlanking());
    this->use20MHzToggle.setValue(settings.use20MHz());
    this->timezoneDropdown.setValue(currentTimezone().name);
    // Spotify account setup. The secret is never sent back to the browser:
    // the card only shows whether one is saved.
    spotifyLogin.setValue("/spotify");
    spotifyClientId.setValue(Spotify::account().clientId.c_str());
    spotifyClientId.onChange([this](const std::optional<dash::string> &value)
                             {
            std::string id = value ? std::string(value->c_str()) : std::string();
            while (!id.empty() && isspace((unsigned char)id.back()))
                id.pop_back();
            while (!id.empty() && isspace((unsigned char)id.front()))
                id.erase(id.begin());
            Spotify::setClientId(id);
            spotifyClientId.setValue(id.c_str());
            showPattern("spotify", true);
            refreshSpotifyStatus(); });
    spotifyClientSecret.setValue(Spotify::account().hasSecret ? "saved" : "");
    spotifyClientSecret.onChange([this](const std::optional<const char *> &value)
                                 {
            std::string secret = value && *value ? std::string(*value) : std::string();
            while (!secret.empty() && isspace((unsigned char)secret.back()))
                secret.pop_back();
            Spotify::setClientSecret(secret);
            spotifyClientSecret.setValue(secret.empty() ? "" : "saved");
            showPattern("spotify", true);
            refreshSpotifyStatus(); });
    spotifyLogout.onPush([this]()
                         {
            Spotify::logOut();
            showPattern("spotify", true);
            refreshSpotifyStatus(); });
    for (dash::Widget *w : std::initializer_list<dash::Widget *>{&spotifyStatus, &spotifyLogin, &spotifyClientId, &spotifyClientSecret, &spotifyLogout})
    {
        w->setTab(spotifyTab);
    }
    // Status is worked out from NVS whenever a browser loads the dashboard.
    dashboard.onBeforeUpdate([this](bool changesOnly)
                             {
        if (!changesOnly)
        {
            refreshSpotifyStatus();
            refreshWeatherStatus();
        } });
    refreshSpotifyStatus();

    WeatherPattern::setLocation(settings.weatherLocation());
    WeatherPattern::setMetric(settings.weatherMetric());
    weatherLocation.setValue(settings.weatherLocation().c_str());
    weatherLocation.onChange([this](const std::optional<dash::string> &value)
                             { setWeatherLocation(value ? std::string(value->c_str()) : std::string()); });
    weatherMetric.setValue(settings.weatherMetric());
    weatherMetric.onChange([this](bool metric)
                           {
            settings.setWeatherMetric(metric);
            WeatherPattern::setMetric(metric);
            weatherMetric.setValue(metric);
            refreshWeatherStatus();
            dashboard.sendUpdates(); });
    for (dash::Widget *w : std::initializer_list<dash::Widget *>{&weatherStatus, &weatherLocation, &weatherMetric})
    {
        w->setTab(weatherTab);
    }
    refreshWeatherStatus();

    Ticker::setMessage(settings.tickerText());
    tickerInput.setValue(settings.tickerText().c_str());
    tickerInput.onChange([this](const std::optional<dash::string> &value)
                         {
            std::string text = value ? std::string(value->c_str()) : std::string();
            if (text.size() > TICKER_MAX_LENGTH)
            {
                text.resize(TICKER_MAX_LENGTH);
            }
            settings.setTickerText(text);
            Ticker::setMessage(text);
            tickerInput.setValue(text.c_str());
            dashboard.sendUpdates(); });
    this->timezoneDropdown.setTab(systemTab);
    {
        // How the last startup went: a warning after a rollback or a boot
        // that never finished starting.
        const BootLog::Entry *previous = bootLog.entry(1);
        const bool trouble = bootLog.rolledBackFrom()[0] || (previous && previous->stage != BootLog::BOOT_CONFIRMED);
        bootStatus.setFeedback(bootLog.summary().c_str(), trouble ? dash::Status::WARNING : dash::Status::SUCCESS);
        bootStatus.setTab(systemTab);
    }

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
            // toInt() returns 0 for garbage, and the cast to uint8_t used to wrap
            // 300 to 44, so check the text and the range before using either.
            const String arg = request->arg("brightness");
            long value = arg.toInt();
            if (arg.length() == 0 || (value == 0 && arg != "0") || value < 0 || value > 255)
            {
                request->send(400, "application/json", "{\"error\": \"brightness must be 0-255\"}");
                return;
            }
            this->setBrightness(value);
            this->brightnessSlider.setValue(value);
            this->dashboard.sendUpdates();
            request->send(200, "application/json", String("{\"brightness\":") + this->getBrightness() + "}");
        }
        else
        {
            request->send(400, "application/json", "{\"error\": \"No brightness parameter\"}");
        } });

    // Render timing for the last 10 s window, plus memory. For tuning; the
    // numbers behind any SIMD or double-buffering decision.
    sprintf(uri, "%s/v1/stats", API_ENDPOINT);
    server.on(uri, HTTP_GET, [&](AsyncWebServerRequest *request)
              {
        Renderer::Stats s = renderer.stats();
        char localTime[32] = "unset";
        struct tm now;
        if (getLocalTime(&now, 0))
        {
            strftime(localTime, sizeof(localTime), "%Y-%m-%d %H:%M:%S %Z", &now);
        }
        static const char *const REASONS[] = {"unknown", "power-on", "external", "software", "panic", "interrupt watchdog",
                                              "task watchdog", "other watchdog", "deep sleep", "brownout", "sdio", "usb",
                                              "jtag", "efuse", "power glitch", "cpu lockup"};
        const int reason = int(esp_reset_reason());
        const char *resetReason = reason >= 0 && reason < int(sizeof(REASONS) / sizeof(REASONS[0])) ? REASONS[reason] : "unknown";
        char body[700];
        snprintf(body, sizeof(body),
                 "{\"reset_reason\":\"%s\",\"rolled_back_from\":\"%s\",\"local_time\":\"%s\",\"timezone\":\"%s\",\"pattern\":\"%s\",\"fps\":%.1f,\"tick_avg_us\":%u,\"tick_max_us\":%u,"
                 "\"push_avg_us\":%u,\"push_max_us\":%u,\"free_internal\":%u,\"largest_internal\":%u,"
                 "\"free_psram\":%u,\"uptime_s\":%lu}",
                 resetReason, bootLog.rolledBackFrom(), localTime, currentTimezone().name, s.pattern, s.windowMs ? s.frames * 1000.0f / s.windowMs : 0.0f,
                 s.tickAvgUs, s.tickMaxUs, s.pushAvgUs, s.pushMaxUs,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                 (unsigned long)(millis() / 1000));
        request->send(200, "application/json", body); });

    // The last few boots, newest first: version, how each started (the
    // previous one's reset reason), whether it was the first boot of an
    // update, and how far startup got. See BootLog.
    sprintf(uri, "%s/v1/boots", API_ENDPOINT);
    server.on(uri, HTTP_GET, [&](AsyncWebServerRequest *request)
              {
        JsonDocument doc;
        doc["rolled_back_from"] = bootLog.rolledBackFrom();
        doc["summary"] = bootLog.summary();
        JsonArray boots = doc["boots"].to<JsonArray>();
        for (int i = 0; i < BootLog::ENTRIES; i++)
        {
            const BootLog::Entry *e = bootLog.entry(i);
            if (e == nullptr)
            {
                break;
            }
            JsonObject b = boots.add<JsonObject>();
            b["version"] = e->version;
            b["started_by"] = BootLog::reasonName(e->reason);
            b["after_update"] = e->afterUpdate != 0;
            b["reached"] = BootLog::stageName(e->stage);
        }
        String body;
        serializeJson(doc, body);
        request->send(200, "application/json", body); });

    // The patterns, and which one is showing. POST with id=<pattern id> to
    // switch, as the dashboard buttons do.
    sprintf(uri, "%s/v1/patterns", API_ENDPOINT);
    server.on(uri, HTTP_GET, [&](AsyncWebServerRequest *request)
              {
        JsonDocument doc;
        doc["current"] = settings.pattern();
        JsonArray list = doc["patterns"].to<JsonArray>();
        for (Pattern *pattern : patternList)
        {
            JsonObject p = list.add<JsonObject>();
            p["id"] = pattern->getId();
            p["name"] = pattern->getName();
        }
        String body;
        serializeJson(doc, body);
        request->send(200, "application/json", body); });
    server.on(uri, HTTP_POST, [&](AsyncWebServerRequest *request)
              {
        const String id = request->hasArg("id") ? request->arg("id") : String();
        for (size_t i = 0; i < std::size(patternList); i++)
        {
            if (id == patternList[i]->getId().c_str())
            {
                renderer.requestPattern(i);
                request->send(202, "application/json", String("{\"requested\":\"") + id + "\"}");
                return;
            }
        }
        request->send(404, "application/json", "{\"error\": \"no pattern with that id\"}"); });

    // The current frame as raw RGB888 (192 x 64, row-major), for capturing
    // what the cube shows: scripts/capture_patterns.py turns it into GIFs.
    // One shared buffer: concurrent captures may see each other's frame.
    sprintf(uri, "%s/v1/frame", API_ENDPOINT);
    server.on(uri, HTTP_GET, [&](AsyncWebServerRequest *request)
              {
        static uint8_t *frame = static_cast<uint8_t *>(heap_caps_malloc(Canvas::FRAME_BYTES, MALLOC_CAP_SPIRAM));
        if (frame == nullptr || !renderer.snapshot(frame, pdMS_TO_TICKS(500)))
        {
            request->send(503, "text/plain", "No frame (is a pattern running?)");
            return;
        }
        // Served straight from the buffer, which outlives the response.
        AsyncWebServerResponse *response = request->beginResponse(200, "application/octet-stream", frame, Canvas::FRAME_BYTES);
        response->addHeader("Cache-Control", "no-store");
        request->send(response); });

    // Spotify account state, for diagnosing a login without a serial console.
    sprintf(uri, "%s/v1/spotify", API_ENDPOINT);
    server.on(uri, HTTP_GET, [&](AsyncWebServerRequest *request)
              {
        const Spotify::Account a = Spotify::account();
        JsonDocument doc;
        doc["client_id_set"] = !a.clientId.empty();
        doc["client_secret_set"] = a.hasSecret;
        doc["linked"] = a.linked;
        doc["error"] = Spotify::lastError();
        String body;
        serializeJson(doc, body);
        request->send(200, "application/json", body); });

    // Login pages. When Spotify hands back a code, show the Spotify pattern:
    // its worker exchanges the code for a token.
    Spotify::registerRoutes(server, [this]()
                            { showPattern("spotify"); });

    // The Ticker pattern's custom message. POST text=<message> (empty to
    // clear); GET returns it.
    sprintf(uri, "%s/v1/ticker", API_ENDPOINT);
    server.on(uri, HTTP_GET, [&](AsyncWebServerRequest *request)
              {
        JsonDocument doc;
        doc["text"] = settings.tickerText();
        String body;
        serializeJson(doc, body);
        request->send(200, "application/json", body); });
    server.on(uri, HTTP_POST, [&](AsyncWebServerRequest *request)
              {
        if (!request->hasArg("text"))
        {
            request->send(400, "application/json", "{\"error\": \"No text parameter\"}");
            return;
        }
        std::string text = request->arg("text").c_str();
        if (text.size() > TICKER_MAX_LENGTH)
        {
            text.resize(TICKER_MAX_LENGTH);
        }
        settings.setTickerText(text);
        Ticker::setMessage(text);
        tickerInput.setValue(text.c_str());
        dashboard.sendUpdates();
        JsonDocument doc;
        doc["text"] = text;
        String body;
        serializeJson(doc, body);
        request->send(200, "application/json", body); });

    // The Weather pattern: GET what it knows; POST location=<place>
    // (empty for the IP address's) and/or metric=true|false, or
    // preview=<WMO code>[&day=false] to show a kind of weather for two
    // minutes (-1 to stop).
    sprintf(uri, "%s/v1/weather", API_ENDPOINT);
    server.on(uri, HTTP_GET, [&](AsyncWebServerRequest *request)
              {
        const WeatherPattern::Report r = WeatherPattern::report();
        JsonDocument doc;
        doc["location_setting"] = settings.weatherLocation();
        doc["metric"] = r.metric;
        doc["located"] = r.located;
        doc["place"] = r.place;
        doc["from_ip"] = r.fromIp;
        doc["current"] = r.current;
        if (r.current)
        {
            doc["temperature"] = weather::temperature(r.tempC, r.metric);
            doc["high"] = weather::temperature(r.highC, r.metric);
            doc["low"] = weather::temperature(r.lowC, r.metric);
            doc["wind"] = weather::windSpeed(r.windKmh, r.metric);
            doc["code"] = r.code;
            doc["conditions"] = weather::describe(r.code);
            doc["day"] = r.day;
            doc["age_s"] = r.ageS;
        }
        doc["error"] = r.error;
        doc["preview"] = r.preview;
        String body;
        serializeJson(doc, body);
        request->send(200, "application/json", body); });
    server.on(uri, HTTP_POST, [&](AsyncWebServerRequest *request)
              {
        if (!request->hasArg("location") && !request->hasArg("metric") && !request->hasArg("preview"))
        {
            request->send(400, "application/json", "{\"error\": \"Give location, metric or preview\"}");
            return;
        }
        if (request->hasArg("preview"))
        {
            WeatherPattern::preview(request->arg("preview").toInt(), request->arg("day") != "false");
            showPattern("weather");
        }
        if (request->hasArg("metric"))
        {
            const bool metric = request->arg("metric") == "true" || request->arg("metric") == "1";
            settings.setWeatherMetric(metric);
            WeatherPattern::setMetric(metric);
            weatherMetric.setValue(metric);
        }
        if (request->hasArg("location"))
        {
            setWeatherLocation(request->arg("location").c_str());
        }
        else
        {
            dashboard.sendUpdates();
        }
        request->send(200, "application/json", "{\"ok\": true}"); });

    // redirect to docs on api root request
    server.on(API_ENDPOINT, HTTP_GET, [&](AsyncWebServerRequest *request)
              { request->redirect("https://github.com/elliotmatson/LED_Cube"); });
}

// set brightness of display
void Cube::setBrightness(uint8_t brightness)
{
    settings.setBrightness(brightness);
    renderer.setBrightness(brightness);
}

// get brightness of display
uint8_t Cube::getBrightness()
{
    return settings.brightness();
}

// set dev mode
void Cube::setDevelopment(bool development)
{
    settings.setDevelopment(development);
}

// set signedFWOnly
void Cube::setSignedFWOnly(bool signedFWOnly)
{
    settings.setSignedFirmwareOnly(signedFWOnly);
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
                        settings.hardware().c_str(),
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
