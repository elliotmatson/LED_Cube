#include "spotify.h"

char scope[] = "user-read-playback-state%20user-modify-playback-state";
char callbackURI[] = "http%3A%2F%2Fcube.local%2Fcallback%2F";
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
          <a href="https://accounts.spotify.com/authorize?client_id=%s&response_type=code&redirect_uri=%s&scope=%s">spotify Auth</a>
          </div>
        </body>
      </html>
      )";

Spotify::Spotify()
{
    data.name = "Spotify";
}

Spotify::~Spotify() 
{
    stop();
}

void Spotify::changeStatus(PatternStatus status)
{
    if (status != patternStatus)
    {
        ESP_LOGI(__func__, "Changing status to %d", status);
        patternStatus = status;
        switch (status)
        {
            case oauth:
                this->pattern->display->fillScreen(0x0000);
                panel0->setCursor(0, 0);
                panel0->print("No OAuth\nCredentials");
                break;
            case refreshToken:
                this->pattern->display->fillScreen(0x0000);
                panel0->setCursor(0, 0);
                panel0->print("Not logged\nin...\n\ncube.local\n/spotify");
                break;
            case noPlayback:
                vTaskSuspend(progressTask);
                this->pattern->display->fillScreen(0x0000);
                break;
            case playback:
                this->pattern->display->fillScreen(0x0000);
                vTaskResume(progressTask);
                break;
        }
    }
}

void Spotify::init(PatternServices *pattern)
{
    this->pattern = pattern;
    panel0 = new SinglePanel(*pattern->display, 2, 2);
    panel1 = new SinglePanel(*pattern->display, 1, 2);
    panel2 = new SinglePanel(*pattern->display, 0, 0);
    spotify = new SpotifyArduino(this->client);
    spotifyPrefs.begin("spotify");
    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback([&](int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t *bitmap)
                        { return this->displayImageOutput(x, y, w, h, bitmap); });

    changeStatus(oauth);
}

void Spotify::start()
{
    xTaskCreate(
        [](void *o)
        { static_cast<Spotify *>(o)->refreshInfo(); }, // This is disgusting, but it works
        "Spotify - Refresh",                    // Name of the task (for debugging)
        10000,                                  // Stack size (bytes)
        this,                                   // Parameter to pass
        1,                                      // Task priority
        &refreshTask                            // Task handle
    );

    xTaskCreate(
        [](void *o)
        { static_cast<Spotify *>(o)->displayProgress(); }, // This is disgusting, but it works
        "Spotify - Display Progress",                      // Name of the task (for debugging)
        2000,                                              // Stack size (bytes)
        this,                                              // Parameter to pass
        1,                                                 // Task priority
        &progressTask                                      // Task handle
    ); 
    vTaskSuspend(progressTask);

    setupCredentials();
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

    changeStatus(refreshToken);

    // Initialize Spotify Library
    ESP_LOGI(__func__, "Setting up Spotify Library");
    client.setCACertBundle(rootca_crt_bundle_start);
    spotify->lateInit(spotifyID, spotifySecret);

    if (spotifyPrefs.getString("SPOTIFY_TOKEN", "").equals(""))
    {
        startOauthWebServer();
        ESP_LOGE(__func__, "No token found");
        return -1;
    } else {
        ESP_LOGI(__func__, "Token found");
        spotify->setRefreshToken(spotifyPrefs.getString("SPOTIFY_TOKEN").c_str());
        changeStatus(noPlayback);
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
    ESP_LOGI(__func__, "Not logged in, Setting up API handlers");
    handlers.push_back(&pattern->server->on("/spotify", HTTP_GET, [&](AsyncWebServerRequest *request)
                                      {    
                    char webpage[800];
                    sprintf(webpage, webpageTemplate, spotifyPrefs.getString("SPOTIFY_ID").c_str(), callbackURI, scope);
                    request->send(200, "text/html", webpage);
                    ESP_LOGI(__func__,"got root request"); }));
    handlers.push_back(&pattern->server->on("/callback/", HTTP_GET, [&](AsyncWebServerRequest *request)
                        {    
                    String code = "";
                    const char *refreshToken = NULL;
                    ESP_LOGI(__func__,"got callback request");
                    for (uint8_t i = 0; i < request->args(); i++)
                    {
                        if (request->argName(i) == "code")
                        {
                            code = request->arg(i);
                            ESP_LOGI(__func__,"got code: %s", code.c_str());
                            refreshToken = spotify->requestAccessTokens(code.c_str(), callbackURI);
                        }
                    }

                    if (refreshToken != NULL)
                    {
                        ESP_LOGI(__func__, "storing token: %s", refreshToken);
                        spotifyPrefs.putString("SPOTIFY_TOKEN", refreshToken);
                        request->send(200, "text/plain", refreshToken);
                        ESP_LOGI(__func__,"got token: %s", refreshToken);
                        spotify->setRefreshToken(refreshToken);
                        this->changeStatus(noPlayback);
                        stopOauthWebServer();
                    }
                    else
                    {
                        request->send(404, "text/plain", "Failed to load token, check serial monitor");
                        ESP_LOGI(__func__,"Failed to load token, check serial monitor");
                    } }));
    ESP_LOGI(__func__, "HTTP server started");
    ESP_LOGI(__func__, "No token found, please visit http://cube.local/spotify to authenticate");
}

void Spotify::stopOauthWebServer()
{   
    ESP_LOGI(__func__, "Removing HTTP server handlers");
    // iterate through handlers vector and remove all handlers
    for (uint8_t i = 0; i < handlers.size(); i++)
    {
        pattern->server->removeHandler(handlers[i]);
    } 
}





void Spotify::stop()
{
    delete panel0;
    delete panel1;
    delete panel2;
    delete spotify;
    vTaskDelete(refreshTask);
    vTaskDelete(progressTask);
}

void Spotify::refreshInfo()
{   
    while (true)
    {   
        int status;
        switch (patternStatus)
        {
            case playback:
                status = spotify->getCurrentlyPlaying([&](CurrentlyPlaying currPlaying)
                                                      { 
                        currentlyPlaying = currPlaying;
                        lastUpdate = millis();
                        if (currentlyPlaying.isPlaying)
                        {
                            String currentTrack = String(currentlyPlaying.trackUri);
                            this->lastPlaying = millis();
                            if (currentTrack.compareTo(previousTrack) != 0)
                            {
                                String currentAlbum = String(currentlyPlaying.albumUri);
                                ESP_LOGI(__func__, "New Track - Updating Text (%s -> %s)\n", this->previousTrack.c_str(), currentlyPlaying.trackUri);
                                displayInfo();
                                if (currentAlbum.compareTo(previousAlbum) != 0)
                                {
                                    ESP_LOGI(__func__, "New Album - Updating Art (%s -> %s)\n", this->previousAlbum.c_str(), currentlyPlaying.albumUri);
                                    displayImage();
                                }
                            }
                        }
                        else if (millis() - lastPlaying > BLANK_AFTER_PAUSE_MS)
                        {
                            changeStatus(noPlayback);
                        }                                    
                        previousTrack = currentlyPlaying.trackUri;
                        previousAlbum = currentlyPlaying.albumUri; });
                if (status > 300)
                {
                    ESP_LOGI(__func__, "Error: %d", status);
                }
                status = spotify->getPlayerDetails([&](PlayerDetails current)
                                                   { 
                                                playerDetails = current;
                                                if (patternStatus == playback)
                                                {
                                                    this->displayPlayback();
                                                } });
                if (status > 300)
                {
                    ESP_LOGI(__func__, "Error: %d", status);
                }
                vTaskDelay(300 / portTICK_PERIOD_MS);
                break;
            case noPlayback:
                status = spotify->getCurrentlyPlaying([&](CurrentlyPlaying currPlaying)
                                                          {      
                        if (currPlaying.isPlaying)
                        {
                            changeStatus(playback);
                            currentlyPlaying = currPlaying;
                            lastUpdate = millis();
                            status = spotify->getPlayerDetails([&](PlayerDetails current)
                                                               { 
                                                playerDetails = current;
                                                this->displayPlayback(); });
                            if (status > 300)
                            {
                                ESP_LOGI(__func__, "Error: %d", status);
                            }
                            displayImage();
                            displayInfo();
                        } });
                if (status > 300)
                {
                    ESP_LOGI(__func__, "Error: %d", status);
                }
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                break;
            default:
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                break;
        }
    }
}

bool Spotify::displayImageOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t *bitmap)
{
    // Stop further decoding as image is running off bottom of screen
    if (y >= panel0->height())
    {
        ESP_LOGI(__func__,"Invalid display parameters: x=%d, y=%d, w=%d, h=%d", x, y, w, h);
        return 0;
    }

    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            panel0->drawPixelRGB888(x + i, y, pgm_read_byte(&bitmap[((j * w + i) * 3)]), pgm_read_byte(&bitmap[((j * w + i) * 3) + 1]), pgm_read_byte(&bitmap[((j * w + i) * 3) + 2]));
        }
    }
    // Return 1 to decode next block
    return 1;
}

int Spotify::displayImage()
{
    SpotifyImage smallestImage = currentlyPlaying.albumImages[currentlyPlaying.numImages - 1];
    String newAlbum = String(smallestImage.url);

    char *albumArtUrl = const_cast<char *>(smallestImage.url);

    uint8_t *imageFile; // pointer that the library will store the image at (uses malloc)
    int imageSize;      // library will update the size of the image
    // log url
    ESP_LOGI(__func__,"Album Art URL: %s", albumArtUrl);
    bool gotImage = spotify->getImage(albumArtUrl, &imageFile, &imageSize);

    if (gotImage)
    {
        int jpegStatus = TJpgDec.drawJpg(0, 0, imageFile, imageSize);
        free(imageFile); // Make sure to free the memory!
        return jpegStatus;
    }
    else
    {
        return -2;
    }
}

void Spotify::displayInfo()
{
    panel1->fillRect(0, 0, 64, 38, 0x0000);
    panel1->setTextColor(0xFFFF);
    panel1->setCursor(0, 0);
    panel1->setTextSize(1);
    panel1->setTextWrap(false);
    panel1->setCursor(0, 5);
    panel1->setFont(&LEMONMILK_Medium7pt7b);
    panel1->println(currentlyPlaying.trackName);
    panel1->setFont(NULL);
    panel1->setCursor(1, 16);
    String artists = "";
    for (int i = 0; i < currentlyPlaying.numArtists; i++)
    {
        artists += currentlyPlaying.artists[i].artistName;
        if (i < currentlyPlaying.numArtists - 1)
        {
            artists += ", ";
        }
    }
    panel1->println(artists);
    panel1->setTextColor(panel1->color565(160, 160, 160));
    panel1->setCursor(1, 27);
    panel1->println(currentlyPlaying.albumName);
}

void Spotify::displayProgress()
{
    while (true)
    {
        if (currentlyPlaying.isPlaying || (millis() - lastUpdate < BLANK_AFTER_PAUSE_MS))
        {
            long progress = currentlyPlaying.progressMs + (millis() - lastUpdate);
            if (!currentlyPlaying.isPlaying)
            {
                progress = currentlyPlaying.progressMs;
            }
            if (progress > currentlyPlaying.durationMs)
            {
                progress = currentlyPlaying.durationMs;
            }
            if (currentlyPlaying.durationMs > 0)
            {
                panel1->drawLine(0, 39, 63, 39, panel1->color565(50, 50, 50));
                int barLength = (progress * 63) / (currentlyPlaying.durationMs);
                long rem = (progress * 63) % (currentlyPlaying.durationMs);
                int bright = (rem * 205 / currentlyPlaying.durationMs) + 50;
                panel1->drawLine(0, 39, barLength, 39, panel1->color565(255, 255, 255));
                panel1->drawPixelRGB888(barLength + 1, 39, bright, bright, bright);
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void Spotify::displayPlayback()
{
    if(playerDetails.isPlaying){
        panel1->drawSprite16(spotify_pause, 24, 44, 16, 16, 100,100,100,true);
    } else {
        panel1->drawSprite16(spotify_play, 24, 44, 16, 16, 100,100,100,true);
    }
    if(playerDetails.shuffleState)
    {
        panel1->drawSprite16(spotify_shuffle_on, 3, 47, 15, 16, 50,120,50,true);
    } else {
        panel1->drawSprite16(spotify_shuffle_off, 3, 47, 15, 16, 100,100,100,true);
    }
    if (playerDetails.repeatState == repeat_off)
    {
        panel1->drawSprite16(spotify_loop_off, 46, 47, 15, 16, 100,100,100,true);
    } else if(playerDetails.repeatState == repeat_context)
    {
        panel1->drawSprite16(spotify_loop_context, 46, 47, 15, 16, 50,120,50,true);
    } else if(playerDetails.repeatState == repeat_track)
    {
        panel1->drawSprite16(spotify_loop_track, 46, 47, 15, 16, 50,120,50,true);
    }
}