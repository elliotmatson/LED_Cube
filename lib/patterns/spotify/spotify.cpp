#include "spotify.h"

char scope[] = "user-read-playback-state%20user-modify-playback-state";

// SPOTIFY_REDIRECT_URI, percent-encoded for the authorize link and the token
// request body. Both must carry exactly the same URI.
static String encodedRedirectUri()
{
    String out;
    for (const char *c = SPOTIFY_REDIRECT_URI; *c; c++)
    {
        if (isalnum((unsigned char)*c) || strchr("-_.~", *c))
        {
            out += *c;
        }
        else
        {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)*c);
            out += hex;
        }
    }
    return out;
}
namespace
{
    // NVS keys, namespace "spotify".
    const char *K_ID = "SPOTIFY_ID";
    const char *K_SECRET = "SPOTIFY_SECRET";
    const char *K_TOKEN = "SPOTIFY_TOKEN";

    // Login state shared by the routes (AsyncTCP task) and the worker.
    // The callback must not block on the token request's TLS handshake, so it
    // hands the code to the worker through pendingCode.
    portMUX_TYPE loginMux = portMUX_INITIALIZER_UNLOCKED;
    char pendingCode[512] = "";
    // "<32 hex>.<cube IP>": random per /spotify visit, checked on /callback/,
    // good for one use within STATE_LIFETIME_MS. The relay page sends the
    // browser back to the IP.
    char oauthState[64] = "";
    uint32_t stateIssuedMs = 0;
    const uint32_t STATE_LIFETIME_MS = 10 * 60 * 1000;

    SemaphoreHandle_t errorLock()
    {
        static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
        return lock;
    }
    std::string lastErrorText;

    const char *PAGE_STYLE =
        "<meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
        "<style>body{font:16px/1.5 system-ui,sans-serif;max-width:32rem;margin:10vh auto;padding:0 16px}"
        "a.button{display:inline-block;background:#1db954;color:#fff;padding:.6em 1.2em;border-radius:2em;text-decoration:none}"
        "p.muted{color:#777}</style>";
}

Spotify::Account Spotify::account()
{
    Preferences prefs;
    Account a;
    if (prefs.begin("spotify", true))
    {
        a.clientId = prefs.getString(K_ID, "").c_str();
        a.hasSecret = prefs.getString(K_SECRET, "").length() > 0;
        a.linked = prefs.getString(K_TOKEN, "").length() > 0;
        prefs.end();
    }
    return a;
}

void Spotify::setClientId(const std::string &id)
{
    Preferences prefs;
    if (prefs.begin("spotify"))
    {
        prefs.putString(K_ID, id.c_str());
        // A token belongs to the app it was issued for.
        prefs.remove(K_TOKEN);
        prefs.end();
    }
    setLastError("");
}

void Spotify::setClientSecret(const std::string &secret)
{
    Preferences prefs;
    if (prefs.begin("spotify"))
    {
        prefs.putString(K_SECRET, secret.c_str());
        prefs.end();
    }
    setLastError("");
}

void Spotify::logOut()
{
    Preferences prefs;
    if (prefs.begin("spotify"))
    {
        prefs.remove(K_TOKEN);
        prefs.end();
    }
    setLastError("");
}

std::string Spotify::lastError()
{
    xSemaphoreTake(errorLock(), portMAX_DELAY);
    std::string copy = lastErrorText;
    xSemaphoreGive(errorLock());
    return copy;
}

void Spotify::setLastError(const std::string &error)
{
    xSemaphoreTake(errorLock(), portMAX_DELAY);
    lastErrorText = error;
    xSemaphoreGive(errorLock());
    if (!error.empty())
    {
        ESP_LOGW("Spotify", "%s", error.c_str());
    }
}

void Spotify::registerRoutes(AsyncWebServer &server, std::function<void()> show)
{
    server.on("/spotify", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        const Account a = account();
        String page = String("<!doctype html><title>Spotify - LED Cube</title>") + PAGE_STYLE + "<h1>Spotify</h1>";
        if (a.clientId.empty() || !a.hasSecret)
        {
            page += "<p>This cube needs your Spotify app's <b>Client ID</b> and <b>Client Secret</b> first. "
                    "Enter them on the cube's dashboard, Spotify tab.</p>"
                    "<p class=muted>Setting up the Spotify app: "
                    "<a href='https://github.com/" REPO_URL "#spotify'>setup guide</a>.</p>";
            request->send(200, "text/html", page);
            return;
        }
        char state[sizeof(oauthState)];
        snprintf(state, sizeof(state), "%08lx%08lx%08lx%08lx.%s", (unsigned long)esp_random(), (unsigned long)esp_random(),
                 (unsigned long)esp_random(), (unsigned long)esp_random(), WiFi.localIP().toString().c_str());
        portENTER_CRITICAL(&loginMux);
        strlcpy(oauthState, state, sizeof(oauthState));
        stateIssuedMs = millis();
        portEXIT_CRITICAL(&loginMux);
        String url = String("https://accounts.spotify.com/authorize?client_id=") + a.clientId.c_str() +
                     "&response_type=code&redirect_uri=" + encodedRedirectUri() + "&scope=" + scope + "&state=" + state;
        page += a.linked ? "<p>A Spotify account is linked. Logging in again replaces it.</p>"
                         : "<p>Link your Spotify account so the cube can show what is playing.</p>";
        page += "<p><a class=button href='" + url + "'>Log in with Spotify</a></p>"
                "<p class=muted>This link works once, for ten minutes.</p>";
        request->send(200, "text/html", page); });

    server.on("/callback/", HTTP_GET, [show](AsyncWebServerRequest *request)
              {
        if (request->hasArg("error"))
        {
            // e.g. access_denied when the user cancels on Spotify's page
            request->send(400, "text/plain", "Spotify login was cancelled (" + request->arg("error") + ").");
            return;
        }
        bool stateOk;
        portENTER_CRITICAL(&loginMux);
        stateOk = oauthState[0] != '\0' && millis() - stateIssuedMs < STATE_LIFETIME_MS &&
                  request->hasArg("state") && request->arg("state") == oauthState;
        if (stateOk)
        {
            oauthState[0] = '\0'; // one use
        }
        portEXIT_CRITICAL(&loginMux);
        if (!stateOk)
        {
            request->send(400, "text/plain", "Login link expired or not from this cube. Start again at /spotify.");
            return;
        }
        if (!request->hasArg("code") || request->arg("code").length() >= sizeof(pendingCode))
        {
            request->send(400, "text/plain", "Spotify did not return a login code.");
            return;
        }
        String code = request->arg("code");
        portENTER_CRITICAL(&loginMux);
        strlcpy(pendingCode, code.c_str(), sizeof(pendingCode));
        portEXIT_CRITICAL(&loginMux);
        show();
        String page = String("<!doctype html><title>Spotify - LED Cube</title>") + PAGE_STYLE +
                      "<h1>Logging in</h1><p>The cube is finishing the login with Spotify and will show what is "
                      "playing in a few seconds. You can close this page.</p>";
        request->send(200, "text/html", page); });
}

Spotify::Spotify()
{
    data.id = "spotify";
    data.name = "Spotify";
}

Spotify::~Spotify()
{
    end();
}

void Spotify::begin(PatternServices *services)
{
    pattern = services;
    if (stateMutex == nullptr)
    {
        stateMutex = xSemaphoreCreateMutex();
        workerDone = xSemaphoreCreateBinary();
    }
    panel0 = new SinglePanel(*pattern->display, 2, 2);
    panel1 = new SinglePanel(*pattern->display, 1, 2);
    spotify = new SpotifyArduino(this->client);
    spotifyPrefs.begin("spotify");
    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback([this](int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t *bitmap)
                        { return this->drawArtPixels(x, y, w, h, bitmap); });

    status = oauth;
    drawnStatus = unknown;
    playingVersion = playerVersion = artVersion = 0;
    drawnPlayingVersion = drawnPlayerVersion = drawnArtVersion = 0;
    playing = NowPlaying();
    player = PlayerState();

    running = true;
    xTaskCreate(
        [](void *o)
        { static_cast<Spotify *>(o)->worker(); },
        "Spotify",
        10240, // TLS
        this,
        1,
        &workerTask);
}

void Spotify::end()
{
    if (workerTask)
    {
        running = false;
        xTaskNotifyGive(workerTask); // cut its wait between polls short
        // A request in flight has to time out first. Deleting the task
        // mid-request would leave TLS and HTTP state behind.
        if (xSemaphoreTake(workerDone, pdMS_TO_TICKS(20000)) != pdTRUE)
        {
            ESP_LOGE(__func__, "Spotify worker did not stop; deleting it");
            vTaskDelete(workerTask);
        }
        workerTask = nullptr;
    }
    delete panel0;
    delete panel1;
    delete spotify;
    panel0 = panel1 = nullptr;
    spotify = nullptr;
    free(art);
    free(shownArt);
    art = shownArt = nullptr;
    artSize = shownArtSize = 0;
    spotifyPrefs.end();
}

void Spotify::setStatus(PatternStatus s)
{
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    if (status != s)
    {
        ESP_LOGI(__func__, "Status %d -> %d", status, s);
        status = s;
    }
    xSemaphoreGive(stateMutex);
}

void Spotify::worker()
{
    setupCredentials();
    while (running)
    {
        exchangePendingCode();
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        PatternStatus s = status;
        xSemaphoreGive(stateMutex);
        if (s == noPlayback || s == playback)
        {
            poll();
        }
        // Woken early by end().
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(SPOTIFY_REQUEST_INTERVAL_MS));
    }
    xSemaphoreGive(workerDone);
    vTaskDelete(NULL);
}

/**
 * One round of requests: what is playing, its album art if the album
 * changed, and the player state. Results go into the shared state for tick().
 */
void Spotify::poll()
{
    bool newAlbum = false;
    String artUrl;
    bool gotItem = false;

    int code = spotify->getCurrentlyPlaying([&](CurrentlyPlaying cp)
                                            {
        gotItem = true;
        NowPlaying np;
        np.trackUri = cp.trackUri;
        np.albumUri = cp.albumUri;
        np.trackName = cp.trackName;
        np.albumName = cp.albumName;
        for (int i = 0; i < cp.numArtists; i++)
        {
            if (i > 0)
            {
                np.artists += ", ";
            }
            np.artists += cp.artists[i].artistName;
        }
        np.progressMs = cp.progressMs;
        np.durationMs = cp.durationMs;
        np.isPlaying = cp.isPlaying;
        np.receivedAtMs = millis();
        // The smallest image is last; local files and some episodes have none.
        if (cp.numImages > 0)
        {
            artUrl = cp.albumImages[cp.numImages - 1].url;
        }

        xSemaphoreTake(stateMutex, portMAX_DELAY);
        newAlbum = np.albumUri != playing.albumUri;
        if (np.trackUri != playing.trackUri)
        {
            playingVersion++;
        }
        playing = np;
        xSemaphoreGive(stateMutex); });
    if (code > 300)
    {
        ESP_LOGW(__func__, "currently-playing: %d", code);
    }

    if (newAlbum && artUrl.length() > 0)
    {
        uint8_t *image = nullptr;
        int size = 0;
        if (spotify->getImage(const_cast<char *>(artUrl.c_str()), &image, &size))
        {
            xSemaphoreTake(stateMutex, portMAX_DELAY);
            free(art);
            art = image;
            artSize = size;
            artVersion++;
            xSemaphoreGive(stateMutex);
        }
    }

    xSemaphoreTake(stateMutex, portMAX_DELAY);
    bool isPlaying = gotItem && playing.isPlaying;
    xSemaphoreGive(stateMutex);
    if (isPlaying)
    {
        lastPlayingMs = millis();
        setStatus(playback);
    }
    else if (millis() - lastPlayingMs > BLANK_AFTER_PAUSE_MS)
    {
        // Paused (or nothing playing, a 204) for long enough to blank.
        setStatus(noPlayback);
    }

    code = spotify->getPlayerDetails([&](PlayerDetails pd)
                                     {
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        player.isPlaying = pd.isPlaying;
        player.shuffle = pd.shuffleState;
        player.repeat = pd.repeatState;
        playerVersion++;
        xSemaphoreGive(stateMutex); });
    if (code > 300)
    {
        ESP_LOGW(__func__, "player: %d", code);
    }
}

int Spotify::setupCredentials()
{
    // Credentials compiled in from secrets.h are defaults for a cube that has
    // none: they no longer overwrite what was entered on the dashboard.
#ifdef SPOTIFY_CLIENT_ID
    if (spotifyPrefs.getString(K_ID, "").length() == 0)
    {
        spotifyPrefs.putString(K_ID, SPOTIFY_CLIENT_ID);
    }
#endif
#ifdef SPOTIFY_CLIENT_SECRET
    if (spotifyPrefs.getString(K_SECRET, "").length() == 0)
    {
        spotifyPrefs.putString(K_SECRET, SPOTIFY_CLIENT_SECRET);
    }
#endif
    // Reset credentials if needed
#ifdef SPOTIFY_RESET_OAUTH
    spotifyPrefs.remove(K_ID);
    spotifyPrefs.remove(K_SECRET);
#endif
#ifdef SPOTIFY_RESET_TOKEN
    spotifyPrefs.remove(K_TOKEN);
#endif
    spotifyPrefs.getString(K_ID, "").toCharArray(spotifyID, 33);
    spotifyPrefs.getString(K_SECRET, "").toCharArray(spotifySecret, 33);

    if (spotifyID[0] == '\0' || spotifySecret[0] == '\0')
    {
        ESP_LOGE(__func__, "Spotify ID or Secret not set");
        return -2;
    }

    setStatus(refreshToken);

    // Initialize Spotify Library
    ESP_LOGI(__func__, "Setting up Spotify Library");
    client.setCACertBundle(rootca_crt_bundle_start, rootca_crt_bundle_end - rootca_crt_bundle_start);
    spotify->lateInit(spotifyID, spotifySecret);

    if (spotifyPrefs.getString(K_TOKEN, "").equals(""))
    {
        ESP_LOGE(__func__, "No token found");
        return -1;
    } else {
        ESP_LOGI(__func__, "Token found");
        spotify->setRefreshToken(spotifyPrefs.getString(K_TOKEN).c_str());
        setStatus(noPlayback);
    }
    ESP_LOGI(__func__, "Refreshing Access Tokens");
    if (!spotify->refreshAccessToken())
    {
        // Revoked, expired, or issued to a different client ID.
        setLastError("Spotify did not accept the saved login; log in again at /spotify.");
        setStatus(refreshToken);
        return -1;
    }
    setLastError("");
    return 1;
}

/**
 * Trades a code from /callback/ for a refresh token. Runs in the worker:
 * the request is a TLS round trip of a second or more.
 */
void Spotify::exchangePendingCode()
{
    char code[sizeof(pendingCode)];
    portENTER_CRITICAL(&loginMux);
    strlcpy(code, pendingCode, sizeof(code));
    pendingCode[0] = '\0';
    portEXIT_CRITICAL(&loginMux);
    if (code[0] == '\0' || spotify == nullptr)
    {
        return;
    }

    const char *refreshToken = spotify->requestAccessTokens(code, encodedRedirectUri().c_str());
    if (refreshToken == NULL)
    {
        setLastError("Spotify rejected the login code. Check the Client Secret, and that the app lists the cube's redirect URI.");
        return;
    }
    // The token is a long-lived credential: stored, never logged.
    spotifyPrefs.putString(K_TOKEN, refreshToken);
    spotify->setRefreshToken(refreshToken);
    ESP_LOGI(__func__, "Logged in to Spotify");
    setLastError("");
    setStatus(noPlayback);
}

void Spotify::tick()
{
    // Copy what changed out of the shared state, and take any new album art,
    // without holding the lock while drawing.
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    PatternStatus s = status;
    bool newPlaying = playingVersion != drawnPlayingVersion;
    bool newPlayer = playerVersion != drawnPlayerVersion;
    NowPlaying np = playing;
    PlayerState pb = player;
    drawnPlayingVersion = playingVersion;
    drawnPlayerVersion = playerVersion;
    if (artVersion != drawnArtVersion)
    {
        free(shownArt);
        shownArt = art;
        shownArtSize = artSize;
        art = nullptr;
        drawnArtVersion = artVersion;
        newPlaying = true; // redraw the art with it
    }
    xSemaphoreGive(stateMutex);

    if (s != drawnStatus)
    {
        drawStatus(s);
        drawnStatus = s;
        // Everything on screen was just cleared.
        newPlaying = newPlayer = true;
    }
    if (s != playback)
    {
        return;
    }
    if (newPlaying)
    {
        drawInfo(np);
        if (shownArt)
        {
            TJpgDec.drawJpg(0, 0, shownArt, shownArtSize);
        }
    }
    if (newPlayer)
    {
        drawPlayback(pb);
    }
    drawProgress(np);
}

void Spotify::drawStatus(PatternStatus s)
{
    pattern->display->fillScreen(0x0000);
    switch (s)
    {
    case oauth:
        panel0->setCursor(0, 0);
        panel0->print("Set up\nSpotify on\nthe cube's\ndashboard");
        break;
    case refreshToken:
        panel0->setCursor(0, 0);
        panel0->print("Log in at\n\ncube.local\n/spotify");
        break;
    default:
        break;
    }
}

bool Spotify::drawArtPixels(int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t *bitmap)
{
    // Stop decoding once the image runs off the bottom of the face.
    if (y >= panel0->height())
    {
        return 0;
    }
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            const uint8_t *px = &bitmap[(j * w + i) * 3];
            panel0->drawPixelRGB888(x + i, y, px[0], px[1], px[2]);
        }
    }
    return 1;
}

void Spotify::drawInfo(const NowPlaying &np)
{
    panel1->fillRect(0, 0, 64, 38, 0x0000);
    panel1->setTextColor(0xFFFF);
    panel1->setTextSize(1);
    panel1->setTextWrap(false);
    panel1->setCursor(0, 5);
    panel1->setFont(&LEMONMILK_Medium7pt7b);
    panel1->println(np.trackName);
    panel1->setFont(NULL);
    panel1->setCursor(1, 16);
    panel1->println(np.artists);
    panel1->setTextColor(panel1->color565(160, 160, 160));
    panel1->setCursor(1, 27);
    panel1->println(np.albumName);
}

void Spotify::drawProgress(const NowPlaying &np)
{
    if (np.durationMs <= 0)
    {
        return;
    }
    // Interpolated between polls while playing.
    int64_t progress = np.progressMs;
    if (np.isPlaying)
    {
        progress += millis() - np.receivedAtMs;
    }
    if (progress > np.durationMs)
    {
        progress = np.durationMs;
    }
    panel1->drawLine(0, 39, 63, 39, panel1->color565(50, 50, 50));
    // 64-bit: progress * 63 overflows a long after about 9.5 hours.
    int barLength = (progress * 63) / np.durationMs;
    int64_t rem = (progress * 63) % np.durationMs;
    int bright = (rem * 205 / np.durationMs) + 50;
    panel1->drawLine(0, 39, barLength, 39, panel1->color565(255, 255, 255));
    panel1->drawPixelRGB888(barLength + 1, 39, bright, bright, bright);
}

void Spotify::drawPlayback(const PlayerState &pb)
{
    if (pb.isPlaying)
    {
        panel1->drawSprite16(spotify_pause, 24, 44, 16, 16, 100, 100, 100, true);
    }
    else
    {
        panel1->drawSprite16(spotify_play, 24, 44, 16, 16, 100, 100, 100, true);
    }
    if (pb.shuffle)
    {
        panel1->drawSprite16(spotify_shuffle_on, 3, 47, 15, 16, 50, 120, 50, true);
    }
    else
    {
        panel1->drawSprite16(spotify_shuffle_off, 3, 47, 15, 16, 100, 100, 100, true);
    }
    if (pb.repeat == repeat_off)
    {
        panel1->drawSprite16(spotify_loop_off, 46, 47, 15, 16, 100, 100, 100, true);
    }
    else if (pb.repeat == repeat_context)
    {
        panel1->drawSprite16(spotify_loop_context, 46, 47, 15, 16, 50, 120, 50, true);
    }
    else if (pb.repeat == repeat_track)
    {
        panel1->drawSprite16(spotify_loop_track, 46, 47, 15, 16, 50, 120, 50, true);
    }
}
