#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <FastLED.h>

#define CHECK_FOR_UPDATES_INTERVAL 5
#ifndef VERSION
  #define VERSION "0.0.0"
#endif

#ifndef REPO_URL
  #define REPO_URL "elliotmatson/LED_Cube"
#endif


#define PANEL_WIDTH 64
#define PANEL_HEIGHT 64
#define PANELS_NUMBER 3
#define PIN_E 32

#define PANE_WIDTH PANEL_WIDTH * PANELS_NUMBER
#define PANE_HEIGHT PANEL_HEIGHT


// placeholder for the matrix object
//MatrixPanel_I2S_DMA dma_display;
MatrixPanel_I2S_DMA *dma_display = nullptr;

int startupBrightness = 0;
int brightness = 100;

unsigned long frameCount=25500;
    uint8_t const cos_wave[256] =  
    {0,0,0,0,1,1,1,2,2,3,4,5,6,6,8,9,10,11,12,14,15,17,18,20,22,23,25,27,29,31,33,35,38,40,42,
    45,47,49,52,54,57,60,62,65,68,71,73,76,79,82,85,88,91,94,97,100,103,106,109,113,116,119,
    122,125,128,131,135,138,141,144,147,150,153,156,159,162,165,168,171,174,177,180,183,186,
    189,191,194,197,199,202,204,207,209,212,214,216,218,221,223,225,227,229,231,232,234,236,
    238,239,241,242,243,245,246,247,248,249,250,251,252,252,253,253,254,254,255,255,255,255,
    255,255,255,255,254,254,253,253,252,252,251,250,249,248,247,246,245,243,242,241,239,238,
    236,234,232,231,229,227,225,223,221,218,216,214,212,209,207,204,202,199,197,194,191,189,
    186,183,180,177,174,171,168,165,162,159,156,153,150,147,144,141,138,135,131,128,125,122,
    119,116,113,109,106,103,100,97,94,91,88,85,82,79,76,73,71,68,65,62,60,57,54,52,49,47,45,
    42,40,38,35,33,31,29,27,25,23,22,20,18,17,15,14,12,11,10,9,8,6,6,5,4,3,2,2,1,1,1,0,0,0,0
    };

unsigned long getUptimeSeconds();
void firmwareUpdate();
void checkForUpdates(void * parameter);
inline uint8_t fastCosineCalc( uint16_t preWrapVal);
inline uint8_t projCalcIntX(uint8_t x, uint8_t y);
inline uint8_t projCalcY(uint8_t x, uint8_t y);

TaskHandle_t checkForUpdatesTask = NULL;


void setup() {
  Serial.begin(500000);
  delay(1000);
  Serial.print("Booting: ");
#ifdef VERSION
  Serial.println(VERSION);
#endif

  HUB75_I2S_CFG mxconfig;
  mxconfig.mx_height = PANEL_HEIGHT;      // we have 64 pix heigh panels
  mxconfig.chain_length = PANELS_NUMBER;  // we have 2 panels chained
  mxconfig.gpio.e = PIN_E;                // we MUST assign pin e to some free pin on a board to drive 64 pix height panels with 1/32 scan
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);

  // let's adjust default brightness to about 75%
  dma_display->setPanelBrightness(100);    // range is 0-255, 0 - 0%, 255 - 100%
  dma_display->setLatBlanking(4);
  

  // Allocate memory and start DMA display
  if( not dma_display->begin() )
      Serial.println("****** !KABOOM! I2S memory allocation failed ***********");


  WiFi.mode(WIFI_STA);
  WiFi.begin();
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  pinMode(2, OUTPUT);

  xTaskCreate(
    checkForUpdates,    // Function that should be called
    "Check For Updates",   // Name of the task (for debugging)
    6000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    0,               // Task priority
    &checkForUpdatesTask             // Task handle
  );
}

void loop(){
  frameCount++ ; 
  uint16_t t = fastCosineCalc((42 * frameCount)>>6);  //time displacement - fiddle with these til it looks good...
  uint16_t t2 = fastCosineCalc((35 * frameCount)>>6); 
  uint16_t t3 = fastCosineCalc((38 * frameCount)>>6);
        
  for (uint8_t i = 0; i < PANE_WIDTH; i++) {
    for (uint8_t j = 0; j < PANE_HEIGHT ; j++) { 
      //frameTime = micros();
      uint8_t x = (projCalcIntX(i,j)+66);
      uint8_t y = (projCalcY(i,j)+66);
      //projTime += micros()-frameTime;

      //frameTime = micros();
      uint8_t r = fastCosineCalc(((x << 3) + (t >> 1) + fastCosineCalc((t2 + (y << 3))))>>2);
      uint8_t g = fastCosineCalc(((y << 3) + t + fastCosineCalc(((t3 >> 2) + (x << 3))))>>2);
      uint8_t b = fastCosineCalc(((y << 3) + t2 + fastCosineCalc((t + x + (g >> 2))))>>2);
      //frameTime = micros()-frameTime;
      //plasmaTime += frameTime;

      //frameTime = micros();
      dma_display->drawPixelRGB888(i,j,r,g,b);
      //frameTime = micros()-frameTime;
      //drawTime += frameTime;
    }
  }
  //double divi = MATRIX_HEIGHT*MATRIX_WIDTH;
  //printf("Proj: %f [us], Plasma: %f [us], Draw: %f [us]\n",projTime/divi, plasmaTime/divi, drawTime/divi);

  if(startupBrightness<brightness){
    startupBrightness++;
    dma_display->setPanelBrightness(startupBrightness);
  }
}


inline uint8_t fastCosineCalc( uint16_t preWrapVal){
  uint8_t wrapVal = (preWrapVal % 255);
  //if (wrapVal<0) wrapVal=255+wrapVal;
  return (cos_wave[wrapVal]); 
}

inline float projCalcX(uint8_t x, uint8_t y){
  if(x<64){       // Panel 1 (top)
    return 0.8660254*(x - y);
  }
  else if(x<128){ // Panel 2 (right)
    return 111.7173 - 0.8660254*x;
  }
  else if(x<192){// Panel 3 (left)
    return 109.1192-0.8660254*x;
  }
} 

inline uint8_t projCalcIntX(uint8_t x, uint8_t y){
  if(x<64){       // Panel 1 (top)
    //return 0.8660254*(x - y);
    return (7*(x - y))>>3;
  }
  else if(x<128){ // Panel 2 (right)
    //return 112 - uint8_t(0.8660254*x);
    return 112 - ((7*x)>>3);
  }
  else if(x<192){// Panel 3 (left)
    //return 109 - uint8_t(0.8660254*x);
    return 109 - ((7*x)>>3);
  }
}
    
inline uint8_t projCalcY(uint8_t x, uint8_t y){
  if(x<64){       // Panel 1 (top)
    return (130-x-y)>>1;
  }
  else if(x<128){ // Panel 2 (right)
    return y-(x>>1);
  }
  else if(x<192){// Panel 3 (left)
    return (x>>1) + y - 128;
  }
}
  

void checkForUpdates(void * parameter){
  for(;;){
    firmwareUpdate();
    vTaskDelay((CHECK_FOR_UPDATES_INTERVAL*1000) / portTICK_PERIOD_MS);
  }
}

void firmwareUpdate(){
#ifdef VERSION
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    String firmwareUrl = String("https://github.com/") + REPO_URL + String("/releases/latest/download/esp32.bin");
    Serial.println(firmwareUrl);
  
    if (!http.begin(client, firmwareUrl))
        return;

    int httpCode = http.sendRequest("HEAD");
    if (httpCode < 300 || httpCode > 400 || http.getLocation().indexOf(String(VERSION)) > 0)
    {
        Serial.printf("Not updating from (sc=%d): %s\n", httpCode, http.getLocation().c_str());
        http.end();
        return;
    }
    else
    {
        Serial.printf("Updating from (sc=%d): %s\n", httpCode, http.getLocation().c_str());
    }

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
#endif
}
