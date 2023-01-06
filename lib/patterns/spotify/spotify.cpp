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

Spotify::Spotify(MatrixPanel_I2S_DMA *display, AsyncWebServer *server) :    panel0(*display, 2, 2),
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
                                            {
                                                this->printCurrentlyPlayingToSerial(current);
                                            },
                                            "US");
    if (status == 200)
    {
        Serial.println("Successfully got currently playing");
    }
    else if (status == 204)
    {
        Serial.println("Doesn't seem to be anything playing");
        panel0.fillScreen(0);
    }
    else
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

    //panel0.drawRGBBitmap(x, y, bitmap, w, h);
    /*
void Adafruit_GFX::drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[],
                                 int16_t w, int16_t h) {
  startWrite();
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      writePixel(x + i, y, pgm_read_word(&bitmap[j * w + i]));
    }
  }
  endWrite();
}
*/

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

void Spotify::printCurrentlyPlayingToSerial(CurrentlyPlaying currentlyPlaying)
{
    if (currentlyPlaying.isPlaying) // was: if (!currentlyPlaying.error) which causes a compiler error 'struct CurrentlyPlaying' has no member named 'error'
    {
        SpotifyImage smallestImage = currentlyPlaying.albumImages[currentlyPlaying.numImages - 1];
        String newAlbum = String(smallestImage.url);
        if (newAlbum != lastAlbumArtUrl)
        {
            Serial.println("Updating Art");
            // int displayImageResult = displayImageUsingFile(smallestImage.url); File reference example
            char *my_url = const_cast<char *>(smallestImage.url); // Modification by @paulsk 2021-10-20
            // convert from const char* to char* // see: https://stackoverflow.com/questions/833034/how-to-convert-const-char-to-char
            int displayImageResult = displayImage(my_url); // was: int displayImageResult =displayImage(smallestImage.url); // Memory Buffer Example - should be much faster
            if (displayImageResult == 0)
            {
                lastAlbumArtUrl = newAlbum;
            }
            else
            {
                Serial.print("failed to display image: ");
                Serial.println(displayImageResult);
            }
        }
        Serial.println("--------- Currently Playing ---------");

        Serial.print("Is Playing: ");
        if (currentlyPlaying.isPlaying)
        {
            Serial.println("Yes");
        }
        else
        {
            Serial.println("No");
        }

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

        long progress = currentlyPlaying.progressMs; // duration passed in the song
        long duration = currentlyPlaying.durationMs; // Length of Song
        Serial.print("Elapsed time of song (ms): ");
        Serial.print(progress);
        Serial.print(" of ");
        Serial.println(duration);
        Serial.println();

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
        Serial.println(">");
        Serial.println();
    }
}