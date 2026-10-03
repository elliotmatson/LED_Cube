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
    histogram = static_cast<Bucket *>(heap_caps_calloc(64, sizeof(Bucket), MALLOC_CAP_SPIRAM));
    ambient = static_cast<float *>(heap_caps_malloc(32 * 32 * sizeof(float), MALLOC_CAP_SPIRAM));
    ambientStartMs = millis();
    frame = 0;
    for (Marquee &m : lines)
    {
        m = Marquee();
    }
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
    free(histogram);
    free(ambient);
    histogram = nullptr;
    ambient = nullptr;
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
    const uint32_t now = millis();
    if (newPlaying)
    {
        drawInfo(np);
        if (shownArt)
        {
            if (histogram)
            {
                memset(histogram, 0, 64 * sizeof(Bucket));
            }
            TJpgDec.drawJpg(0, 0, shownArt, shownArtSize);
            pickArtPalette();
        }
    }
    else
    {
        for (Marquee &m : lines)
        {
            stepMarquee(m, now);
        }
    }
    if (newPlayer)
    {
        drawPlayback(pb);
        shownPlaying = pb.isPlaying;
    }
    drawProgress(np);
    // Every other tick: the cloud moves slowly, and this keeps the push small.
    if ((frame++ & 1) == 0)
    {
        drawAmbient(shownPlaying);
    }
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
            if (histogram)
            {
                Bucket &b = histogram[(px[0] >> 6) * 16 + (px[1] >> 6) * 4 + (px[2] >> 6)];
                b.count++;
                b.r += px[0];
                b.g += px[1];
                b.b += px[2];
            }
        }
    }
    return 1;
}

void Spotify::drawInfo(const NowPlaying &np)
{
    // Same positions and fonts as before; each line now scrolls if it does
    // not fit, within its own band of rows. The title's baseline is 11: the
    // old code set the font after the cursor, and GFX moves the cursor down 6
    // when switching to a custom font. Its glyphs span baseline -12 to +3.
    panel1->fillRect(0, 0, 64, 38, 0x0000);
    setMarquee(lines[0], np.trackName, &LEMONMILK_Medium7pt7b, 0, 11, 0, 15, 0xFFFF);
    setMarquee(lines[1], np.artists, nullptr, 1, 16, 15, 10, 0xFFFF);
    setMarquee(lines[2], np.albumName, nullptr, 1, 27, 26, 10, panel1->color565(160, 160, 160));
}

void Spotify::setMarquee(Marquee &m, const String &text, const GFXfont *font, int16_t x, int16_t y,
                         int16_t bandTop, int16_t bandHeight, uint16_t color)
{
    m.text = text;
    m.font = font;
    m.x = x;
    m.y = y;
    m.color = color;
    m.bandTop = bandTop;
    m.bandHeight = bandHeight;
    panel1->setFont(font);
    panel1->setTextSize(1);
    panel1->setTextWrap(false);
    int16_t x1, y1;
    uint16_t w, h;
    panel1->getTextBounds(text.c_str(), 0, y, &x1, &y1, &w, &h);
    m.width = int16_t(w) + x1;
    m.scrolls = m.width > cube::FACE_SIZE - x;
    m.offset = 0;
    m.pauseUntil = millis() + 2500;
    drawMarquee(m);
}

void Spotify::drawMarquee(Marquee &m)
{
    // The gap before the text comes round again.
    const int16_t GAP = 24;
    panel1->fillRect(0, m.bandTop, cube::FACE_SIZE, m.bandHeight, 0x0000);
    panel1->setFont(m.font);
    panel1->setTextSize(1);
    panel1->setTextWrap(false);
    panel1->setTextColor(m.color);
    panel1->setCursor(m.x - m.offset, m.y);
    panel1->print(m.text);
    if (m.scrolls)
    {
        panel1->setCursor(m.x - m.offset + m.width + GAP, m.y);
        panel1->print(m.text);
        if (m.offset >= m.width + GAP)
        {
            m.offset = 0; // the second copy is now where the first started
        }
    }
    panel1->setFont(NULL);
}

void Spotify::stepMarquee(Marquee &m, uint32_t now)
{
    if (!m.scrolls || int32_t(now - m.pauseUntil) < 0)
    {
        return;
    }
    m.offset++;
    drawMarquee(m);
    if (m.offset == 0)
    {
        m.pauseUntil = now + 2500; // back at the start: pause again
    }
}

void Spotify::pickArtPalette()
{
    if (!histogram)
    {
        return;
    }
    // Score each colour bucket by how much of the art it covers, favouring
    // saturated colours over greys; skip near-black. Take the best three,
    // as the average colour of each bucket.
    float score[64];
    for (int i = 0; i < 64; i++)
    {
        const Bucket &b = histogram[i];
        if (b.count == 0)
        {
            score[i] = -1;
            continue;
        }
        const uint8_t r = b.r / b.count, g = b.g / b.count, bl = b.b / b.count;
        const uint8_t hi = max(r, max(g, bl)), lo = min(r, min(g, bl));
        score[i] = hi < 40 ? -1 : b.count * (0.25f + float(hi - lo) / 255);
    }
    int found = 0;
    for (int k = 0; k < 3; k++)
    {
        int best = -1;
        for (int i = 0; i < 64; i++)
        {
            if (score[i] > 0 && (best < 0 || score[i] > score[best]))
            {
                best = i;
            }
        }
        if (best < 0)
        {
            break;
        }
        const Bucket &b = histogram[best];
        artPalette[found++] = {uint8_t(b.r / b.count), uint8_t(b.g / b.count), uint8_t(b.b / b.count)};
        score[best] = -1;
    }
    // Monochrome art: fill the rest with brighter versions of what there is.
    for (int k = found; k < 3; k++)
    {
        const color::RGB c = found ? artPalette[k - 1] : color::RGB{30, 215, 96};
        artPalette[k] = color::lerp(c, {255, 255, 255}, 70);
    }
}

void Spotify::drawAmbient(bool playing)
{
    if (!ambient)
    {
        return;
    }
    // Noise sampled at every other pixel and interpolated, as in Nebula; the
    // top face is the z = 64 plane, so its own (x, y) are the coordinates.
    const float t = (millis() - ambientStartMs) * 0.00012f;
    for (int sy = 0; sy < 32; sy++)
    {
        for (int sx = 0; sx < 32; sx++)
        {
            ambient[sy * 32 + sx] = noise::fbm(sx * 0.09f + t, sy * 0.09f - t * 0.6f, t * 0.5f, 2);
        }
    }
    const color::RGB stops[4] = {{0, 0, 0}, artPalette[0], artPalette[1], artPalette[2]};
    const uint8_t level = playing ? 255 : 90; // dim while paused
    for (int16_t y = 0; y < cube::FACE_SIZE; y++)
    {
        const int sy0 = y / 2, sy1 = (y & 1) && sy0 < 31 ? sy0 + 1 : sy0;
        uint8_t *out = pattern->display->rowForWrite(y, 0, cube::FACE_SIZE);
        for (int16_t x = 0; x < cube::FACE_SIZE; x++)
        {
            const int sx0 = x / 2, sx1 = (x & 1) && sx0 < 31 ? sx0 + 1 : sx0;
            const float n = 0.25f * (ambient[sy0 * 32 + sx0] + ambient[sy0 * 32 + sx1] +
                                     ambient[sy1 * 32 + sx0] + ambient[sy1 * 32 + sx1]);
            int v = int((n * 1.1f + 0.45f) * 255);
            v = v < 0 ? 0 : (v > 255 ? 255 : v);
            const color::RGB c = color::scale(color::gradient(stops, 4, uint8_t(v)), level);
            *out++ = c.r;
            *out++ = c.g;
            *out++ = c.b;
        }
    }
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
