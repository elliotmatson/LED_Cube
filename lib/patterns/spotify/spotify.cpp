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
const char *webpageTemplate =
    R"(
      <!DOCTYPE html>
      <html>
        <head>
          <meta charset="utf-8">
          <meta http-equiv="X-UA-Compatible" content="IE=edge">
          <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no" />
        </head>
        <body>
          <div>
          <a href="https://accounts.spotify.com/authorize?client_id=%s&response_type=code&redirect_uri=%s&scope=%s&state=%s">spotify Auth</a>
          </div>
        </body>
      </html>
      )";

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

    // Registered for as long as the pattern runs, whether or not a token is
    // stored, so a revoked token can be replaced by logging in again.
    startOauthWebServer();

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
    if (pattern && !handlers.empty())
    {
        stopOauthWebServer();
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
    // Get Spotify Credentials
#ifdef SPOTIFY_CLIENT_ID
    spotifyPrefs.putString("SPOTIFY_ID", SPOTIFY_CLIENT_ID);
#endif
#ifdef SPOTIFY_CLIENT_SECRET
    spotifyPrefs.putString("SPOTIFY_SECRET", SPOTIFY_CLIENT_SECRET);
#endif
    // Reset credentials if needed
#ifdef SPOTIFY_RESET_OAUTH
    spotifyPrefs.remove("SPOTIFY_ID");
    spotifyPrefs.remove("SPOTIFY_SECRET");
#endif
#ifdef SPOTIFY_RESET_TOKEN
    spotifyPrefs.remove("SPOTIFY_TOKEN");
#endif
    spotifyPrefs.getString("SPOTIFY_ID", "").toCharArray(spotifyID, 33);
    spotifyPrefs.getString("SPOTIFY_SECRET", "").toCharArray(spotifySecret, 33);

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

    if (spotifyPrefs.getString("SPOTIFY_TOKEN", "").equals(""))
    {
        ESP_LOGE(__func__, "No token found");
        return -1;
    } else {
        ESP_LOGI(__func__, "Token found");
        spotify->setRefreshToken(spotifyPrefs.getString("SPOTIFY_TOKEN").c_str());
        setStatus(noPlayback);
    }
    ESP_LOGI(__func__, "Refreshing Access Tokens");
    if (!spotify->refreshAccessToken())
    {
        ESP_LOGI(__func__, "Failed to get access tokens");
    }
    return 1;
}

void Spotify::startOauthWebServer()
{
    ESP_LOGI(__func__, "Setting up Spotify login handlers");
    handlers.push_back(&pattern->server->on("/spotify", HTTP_GET, [this](AsyncWebServerRequest *request)
                                            {
                    // Random, then where the relay page should send the browser
                    // back to. The IP rather than cube.local: the relay accepts
                    // either, but not every client resolves mDNS.
                    snprintf(oauthState, sizeof(oauthState), "%08lx%08lx.%s", (unsigned long)esp_random(), (unsigned long)esp_random(), WiFi.localIP().toString().c_str());
                    String spotifyId = spotifyPrefs.getString("SPOTIFY_ID");
                    if (spotifyId.length() == 0)
                    {
                        request->send(503, "text/plain", "This cube has no Spotify client ID configured.");
                        return;
                    }
                    char webpage[1000];
                    snprintf(webpage, sizeof(webpage), webpageTemplate, spotifyId.c_str(), encodedRedirectUri().c_str(), scope, oauthState);
                    request->send(200, "text/html", webpage); }));
    handlers.push_back(&pattern->server->on("/callback/", HTTP_GET, [this](AsyncWebServerRequest *request)
                                            {
                    ESP_LOGI(__func__, "got callback request");
                    if (request->hasArg("error"))
                    {
                        // e.g. access_denied when the user cancels on Spotify's page
                        request->send(400, "text/plain", "Spotify login was cancelled (" + request->arg("error") + ").");
                        return;
                    }
                    if (oauthState[0] == '\0' || !request->hasArg("state") || request->arg("state") != oauthState)
                    {
                        request->send(400, "text/plain", "Login link expired or not from this cube. Start again at /spotify.");
                        return;
                    }
                    if (!request->hasArg("code") || request->arg("code").length() >= sizeof(pendingCode))
                    {
                        request->send(400, "text/plain", "Spotify did not return a login code.");
                        return;
                    }
                    oauthState[0] = '\0'; // one use
                    String code = request->arg("code");
                    portENTER_CRITICAL(&codeMux);
                    strlcpy(pendingCode, code.c_str(), sizeof(pendingCode));
                    portEXIT_CRITICAL(&codeMux);
                    request->send(200, "text/plain", "Logging the cube in to Spotify. You can close this page."); }));
    ESP_LOGI(__func__, "Visit http://cube.local/spotify to log in");
}

void Spotify::stopOauthWebServer()
{
    ESP_LOGI(__func__, "Removing HTTP server handlers");
    for (AsyncCallbackWebHandler *handler : handlers)
    {
        pattern->server->removeHandler(handler);
    }
    // removeHandler() destroys them; keeping the pointers would remove freed
    // handlers on the next stop.
    handlers.clear();
}

/**
 * Trades a code from /callback/ for a refresh token. Runs in the worker:
 * the request is a TLS round trip of a second or more.
 */
void Spotify::exchangePendingCode()
{
    char code[sizeof(pendingCode)];
    portENTER_CRITICAL(&codeMux);
    strlcpy(code, pendingCode, sizeof(code));
    pendingCode[0] = '\0';
    portEXIT_CRITICAL(&codeMux);
    if (code[0] == '\0' || spotify == nullptr)
    {
        return;
    }

    const char *refreshToken = spotify->requestAccessTokens(code, encodedRedirectUri().c_str());
    if (refreshToken == NULL)
    {
        ESP_LOGE(__func__, "Spotify rejected the login code");
        return;
    }
    // The token is a long-lived credential: stored, never logged.
    spotifyPrefs.putString("SPOTIFY_TOKEN", refreshToken);
    spotify->setRefreshToken(refreshToken);
    ESP_LOGI(__func__, "Logged in to Spotify");
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
        panel0->print("No OAuth\nCredentials");
        break;
    case refreshToken:
        panel0->setCursor(0, 0);
        panel0->print("Not logged\nin...\n\ncube.local\n/spotify");
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
