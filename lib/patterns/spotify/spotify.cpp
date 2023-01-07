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

Spotify::Spotify(MatrixPanel_I2S_DMA *display, AsyncWebServer *server) : panel0(*display, 2, 2),
                                                                         panel1(*display, 1, 2),
                                                                         panel2(*display, 0, 0),
                                                                         spotify(this->client)
{
  this->display = display;
  this->server = server;
}

void Spotify::init(){
    spotifyPrefs.begin("spotify");

    spotifyPrefs.getString("SPOTIFY_ID").toCharArray(spotifyID, 33);
    spotifyPrefs.getString("SPOTIFY_SECRET").toCharArray(spotifySecret, 33);

    Serial.println("Setting up Spotify Library");
    client.setCACertBundle(rootca_crt_bundle_start);
    spotify.lateInit(spotifyID, spotifySecret, spotifyPrefs.getString("SPOTIFY_TOKEN").c_str());


    Serial.println("Setting up callbacks");
    server->on("/spotify", HTTP_GET, [&](AsyncWebServerRequest *request)
               {    
                    char webpage[800];
                    sprintf(webpage, webpageTemplate, spotifyPrefs.getString("SPOTIFY_ID").c_str(), callbackURI, scope);
                    request->send(200, "text/html", webpage);
                    Serial.printf("got root request"); });
    server->on("/callback/", HTTP_GET, [&](AsyncWebServerRequest *request)
               {    
                    String code = "";
                    const char *refreshToken = NULL;
                    for (uint8_t i = 0; i < request->args(); i++)
                    {
                        if (request->argName(i) == "code")
                        {
                            code = request->arg(i);
                            refreshToken = spotify.requestAccessTokens(code.c_str(), callbackURI);
                        }
                    }

                    if (refreshToken != NULL)
                    {
                        spotifyPrefs.putString("SPOTIFY_TOKEN", refreshToken);
                        request->send(200, "text/plain", refreshToken);
                        Serial.printf("got token: %s", refreshToken);
                    }
                    else
                    {
                        request->send(404, "text/plain", "Failed to load token, check serial monitor");
                        Serial.printf("Failed to load token, check serial monitor");
                    } });
    server->onNotFound([&](AsyncWebServerRequest *request)
                    {
                        String message = "File Not Found\n\n";
                        message += "URI: ";
                        message += request->url();
                        message += "\nMethod: ";
                        message += (request->method() == HTTP_GET) ? "GET" : "POST";
                        message += "\nArguments: ";
                        message += request->args();
                        message += "\n";

                        for (uint8_t i = 0; i < request->args(); i++)
                        {
                            message += " " + request->argName(i) + ": " + request->arg(i) + "\n";
                        }

                        Serial.print(message);
                        request->send(404, "text/plain", message);

                        Serial.printf(message.c_str());
                    });
    Serial.println("HTTP server started");
    TJpgDec.setJpgScale(1);

    // The decoder must be given the exact name of the rendering function above
    TJpgDec.setCallback([&](int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t *bitmap)
                        {
                            return this->displayOutput(x, y, w, h, bitmap);
                        });

    Serial.println("Refreshing Access Tokens");
    if (!spotify.refreshAccessToken())
    {
        Serial.println("Failed to get access tokens");
    }
}

void Spotify::show()
{
    // Market can be excluded if you want e.g. spotify.getCurrentlyPlaying()
    int status = spotify.getCurrentlyPlaying([&](CurrentlyPlaying current)
                                             { this->printCurrentlyPlayingToSerial(current); },
                                             "US");
    if (status > 300)
    {
        Serial.print("Error: ");
        Serial.println(status);
    }
    status = spotify.getPlayerDetails([&](PlayerDetails current)
                                             { this->displayPlayback(current); },
                                             "US");
    if (status > 300)
    {
        Serial.print("Error: ");
        Serial.println(status);
    }
}

bool Spotify::displayOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t *bitmap)
{
    // Stop further decoding as image is running off bottom of screen
    if (y >= panel0.height()){
        Serial.printf("Invalid display parameters: x=%d, y=%d, w=%d, h=%d", x, y, w, h);
        return 0;
    }

    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            panel0.drawPixelRGB888(x + i, y, pgm_read_byte(&bitmap[((j * w + i) * 3)]), pgm_read_byte(&bitmap[((j * w + i) * 3)+1]), pgm_read_byte(&bitmap[((j * w + i) * 3)+2]));
        }
    }
    // Return 1 to decode next block
    return 1;
}

int Spotify::displayImage(char *albumArtUrl)
{

    uint8_t *imageFile; // pointer that the library will store the image at (uses malloc)
    int imageSize;      // library will update the size of the image
    bool gotImage = spotify.getImage(albumArtUrl, &imageFile, &imageSize);

    if (gotImage)
    {
        delay(1);
        int jpegStatus = TJpgDec.drawJpg(0, 0, imageFile, imageSize);
        free(imageFile); // Make sure to free the memory!
        return jpegStatus;
    }
    else
    {
        return -2;
    }
}

void Spotify::displayInfo(CurrentlyPlaying currentlyPlaying)
{
    for (int i = 0; i < 64; i++)
    {
        for (int j = 0; j < 40; j++)
        {
            panel1.drawPixel(i, j, 0x0000);
        }
    }
    panel1.setTextColor(0xFFFF);
    panel1.setCursor(0, 0);
    panel1.setTextSize(1);
    panel1.setTextWrap(false);
    panel1.setCursor(0, 5);
    panel1.setFont(&LEMONMILK_Medium7pt7b);
    panel1.println(currentlyPlaying.trackName);
    panel1.setFont(NULL);
    panel1.setCursor(1, 16);
    String artists = "";
    for (int i = 0; i < currentlyPlaying.numArtists; i++)
    {
        artists += currentlyPlaying.artists[i].artistName;
        if (i < currentlyPlaying.numArtists - 1)
        {
            artists += ", ";
        }
    }
    panel1.println(artists);
    panel1.setTextColor(panel1.color565(160, 160, 160));
    panel1.setCursor(1, 27);
    panel1.println(currentlyPlaying.albumName);
    panel1.drawLine(0, 39, 63, 39, panel1.color565(127, 127, 127));
    panel2.setRotation(0);
}

void Spotify::displayPlayback(PlayerDetails playerDetails)
{
    if(playerDetails.isPlaying){
        panel1.drawSprite16(spotify_pause, 24, 44, 16, 16, 100,100,100,true);
    } else {
        panel1.drawSprite16(spotify_play, 24, 44, 16, 16, 100,100,100,true);
    }
    if(playerDetails.shuffleState)
    {
        panel1.drawSprite16(spotify_shuffle_on, 3, 47, 15, 16, 50,120,50,true);
    } else {
        panel1.drawSprite16(spotify_shuffle_off, 3, 47, 15, 16, 100,100,100,true);
    }
    if (playerDetails.repeatState == repeat_off)
    {
        panel1.drawSprite16(spotify_loop_off, 46, 47, 15, 16, 100,100,100,true);
    } else if(playerDetails.repeatState == repeat_context)
    {
        panel1.drawSprite16(spotify_loop_context, 46, 47, 15, 16, 50,120,50,true);
    } else if(playerDetails.repeatState == repeat_track)
    {
        panel1.drawSprite16(spotify_loop_track, 46, 47, 15, 16, 50,120,50,true);
    }
}

void Spotify::printCurrentlyPlayingToSerial(CurrentlyPlaying currentlyPlaying)
{
    if (currentlyPlaying.isPlaying) // was: if (!currentlyPlaying.error) which causes a compiler error 'struct CurrentlyPlaying' has no member named 'error'
    {
        String currentTrack = String(currentlyPlaying.trackUri);
        if (currentTrack.compareTo(previousTrack) != 0)
        {
            String currentAlbum = String(currentlyPlaying.albumUri);
            Serial.printf("New Track - Updating Text (%s -> %s)\n", previousTrack.c_str(), currentlyPlaying.trackUri);
            displayInfo(currentlyPlaying);
            if (currentAlbum.compareTo(previousAlbum) != 0)
            {
                Serial.printf("New Album - Updating Art (%s -> %s)\n", previousAlbum.c_str(), currentlyPlaying.albumUri);

                SpotifyImage smallestImage = currentlyPlaying.albumImages[currentlyPlaying.numImages - 1];
                String newAlbum = String(smallestImage.url);

                char *my_url = const_cast<char *>(smallestImage.url);
                int displayImageResult = displayImage(my_url);
                if (displayImageResult != 0)
                {
                    Serial.print("failed to display image: ");
                    Serial.println(displayImageResult);
                }
            }
            Serial.println("--------- Currently Playing ---------");

            Serial.print("Track: ");
            Serial.println(currentlyPlaying.trackName);
            Serial.println();

            Serial.println("Artists: ");
            for (int i = 0; i < currentlyPlaying.numArtists; i++)
            {
                Serial.print("Name: ");
                Serial.println(currentlyPlaying.artists[i].artistName);
                Serial.println();
            }

            Serial.print("Album: ");
            Serial.println(currentlyPlaying.albumName);
            Serial.println();
        }
        long progress = currentlyPlaying.progressMs; // duration passed in the song
        long duration = currentlyPlaying.durationMs; // Length of Song
        /*
        Serial.print("Elapsed time of song (ms): ");
        Serial.print(progress);
        Serial.print(" of ");
        Serial.println(duration);
        Serial.println();
        */

        float percentage = ((float)progress / (float)duration) * 100;
        int clampedPercentage = (int)percentage;
        Serial.print("<");
        for (int j = 0; j < 50; j++)
        {
            if (clampedPercentage >= (j * 2))
            {
                Serial.print("=");
            }
            else
            {
                Serial.print("-");
            }
        }
        Serial.printf(">\r");
        previousTrack = currentlyPlaying.trackUri;
        previousAlbum = currentlyPlaying.albumUri;
    }
}