#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

#include <WiFiManager.h>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <FastLED.h>

#define CHECK_FOR_UPDATES_INTERVAL 5
//#ifndef VERSION
//  #define VERSION "v0.0.6"
//#endif


#ifndef REPO_URL
  #define REPO_URL "elliotmatson/LED_Cube"
#endif



#define PANEL_WIDTH 64
#define PANEL_HEIGHT 64
#define PANELS_NUMBER 3

// new PCB pinouts

#define R1_PIN 4
#define G1_PIN 15
#define B1_PIN 5
#define R2_PIN 19
#define G2_PIN 18
#define B2_PIN 22
#define A_PIN 32
#define B_PIN 23
#define C_PIN 33
#define D_PIN 14
#define E_PIN 21
#define LAT_PIN 27
#define OE_PIN 26
#define CLK_PIN 25


// placeholder for the matrix object
//MatrixPanel_I2S_DMA dma_display;
MatrixPanel_I2S_DMA *dma_display = nullptr;

//int startupBrightness = 0;
//int brightness = 100;

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

void firmwareUpdate();
void checkForUpdates(void * parameter);
void checkForOTA(void * parameter);
void drawFrame(void * parameter);
void checkStats(void * parameter);
inline uint8_t fastCosineCalc( uint16_t preWrapVal);
inline uint8_t projCalcIntX(uint8_t x, uint8_t y);
inline uint8_t projCalcY(uint8_t x, uint8_t y);
void showConfigScreen();
void scrollString(String str);

TaskHandle_t checkForUpdatesTask = NULL;
TaskHandle_t checkForOTATask = NULL;
TaskHandle_t drawFrameTask = NULL;
TaskHandle_t checkStatsTask = NULL;


void setup() {
  Serial.begin(115200);
  Serial.printf("Booting: \n");
  Serial.printf("XTAL=%d, CPU=%d, APB=%d\n", getXtalFrequencyMhz(), getCpuFrequencyMhz(), getApbFrequency());
  if(psramInit()){
    Serial.println("PSRAM is correctly initialized");
  }else{
    Serial.println("PSRAM not available");
  }

  Serial.printf("Connecting to WiFi...\n");
  WiFiManager wifiManager;
  wifiManager.setHostname("cube");
  wifiManager.setClass("invert");
  wifiManager.autoConnect("Cube");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

#ifdef VERSION
  Serial.printf("FW Version: " + VERSION);
  Serial.printf("Github Update enabled...\n");

  xTaskCreate(
    checkForUpdates,    // Function that should be called
    "Check For Updates",   // Name of the task (for debugging)
    6000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    0,               // Task priority
    &checkForUpdatesTask             // Task handle
  );
#else
  Serial.printf("OTA Update Enabled\n");
  ArduinoOTA.setHostname("cube");
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  xTaskCreate(
    checkForOTA,    // Function that should be called
    "Check For OTA",   // Name of the task (for debugging)
    6000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    0,               // Task priority
    &checkForOTATask             // Task handle
  );
#endif

  Serial.printf("Configuring HUB_75\n");
  HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
  HUB75_I2S_CFG mxconfig(
  	PANEL_WIDTH, // Module width
  	PANEL_HEIGHT, // Module height
  	PANELS_NUMBER, // chain length
  	_pins // pin mapping
  );
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;
  mxconfig.clkphase = false; // Change this if you have issues with ghosting.
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A; // Change this according to your pane.
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);

  // let's adjust default brightness to about 75%
  dma_display->setBrightness8(255);    // range is 0-255, 0 - 0%, 255 - 100%
  //dma_display->setLatBlanking(2);
  

  // Allocate memory and start DMA display
  if( not dma_display->begin() )
      Serial.println("****** !KABOOM! I2S memory allocation failed ***********");

  /*xTaskCreate(
    drawFrame,    // Function that should be called
    "Draw Frame",   // Name of the task (for debugging)
    10000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    &drawFrameTask             // Task handle
  );*/

  Serial.printf("Ready. Starting loop\n");

  xTaskCreate(
    checkStats,    // Function that should be called
    "Draw Frame",   // Name of the task (for debugging)
    5000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    &checkStatsTask             // Task handle
  );
}

void loop(){
  for (uint8_t i = 0; i < 100; i++) {
    dma_display->drawPixelRGB888(random(0,64*3),random(0,64),255,255,255);
    dma_display->drawPixelRGB888(random(0,64*3),random(0,64),0,0,0);
  }
  delay(100);
}

void plasma()
{
  frameCount++ ; 
  uint16_t t = fastCosineCalc((42 * frameCount)>>6);  //time displacement - fiddle with these til it looks good...
  uint16_t t2 = fastCosineCalc((35 * frameCount)>>6); 
  uint16_t t3 = fastCosineCalc((38 * frameCount)>>6);
//  double frameTime, projTime, plasmaTime, drawTime = 0;
        
  for (uint8_t i = 0; i < PANEL_WIDTH*PANELS_NUMBER; i++) {
    for (uint8_t j = 0; j < PANEL_HEIGHT ; j++) { 
//      frameTime = micros();
      uint8_t x = (projCalcIntX(i,j)+66);
      uint8_t y = (projCalcY(i,j)+66);
//      projTime += micros()-frameTime;

//      frameTime = micros();
      uint8_t r = fastCosineCalc(((x << 3) + (t >> 1) + fastCosineCalc((t2 + (y << 3))))>>2);
      uint8_t g = fastCosineCalc(((y << 3) + t + fastCosineCalc(((t3 >> 2) + (x << 3))))>>2);
      uint8_t b = fastCosineCalc(((y << 3) + t2 + fastCosineCalc((t + x + (g >> 2))))>>2);
//      frameTime = micros()-frameTime;
//      plasmaTime += frameTime;

//      frameTime = micros();
      dma_display->drawPixelRGB888(i,j,r,g,b);
//      frameTime = micros()-frameTime;
//      drawTime += frameTime;
    }
  }
//  double divi = MATRIX_HEIGHT*192;
//  printf("Proj: %f [us], Plasma: %f [us], Draw: %f [us]\n",projTime/divi, plasmaTime/divi, drawTime/divi);
  //if(startupBrightness<brightness){
  //  startupBrightness++;
  //  dma_display->setPanelBrightness(startupBrightness);
  //}
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
  
void showConfigScreen(){
  dma_display->fillScreenRGB888(0, 0, 0);
  dma_display->setCursor(1, 1);
  dma_display->setTextColor(0xFFFF);
  dma_display->setTextSize(1);
  dma_display->print("WiFi");
  scrollString("WiFi WiFi WiFi WiFi WiFi WiFi WiFi WiFi WiFi ");
}
void scrollString(String str) {
    int yoff = 1;
    dma_display->fillScreenRGB888(0, 0, 0);
    dma_display->setTextWrap(false);  // we don't wrap text so it scrolls nicely
    dma_display->setTextSize(1);
    dma_display->setRotation(1);
    int charWidth = 12; // textsize 2 @todo auto calculate charwidth from font
    int pxwidth = (str.length()*charWidth)+32; // @todo get actual string pixel length, add support to gfx if needed
    for (int32_t x=charWidth; x>=-pxwidth; x--) {
        dma_display->fillScreenRGB888(0, 0, 0);
        dma_display->setCursor(x,yoff);
        // Serial.println((String)x);
        // display.print("ABCDEFGHIJKLMNONPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789");
        dma_display->print(str);
        // delay(ANIMSPEED/2);
        delay(150);
    }
}

void drawFrame(void * parameter){
  for(;;){
    plasma();
    vTaskDelay(33 / portTICK_PERIOD_MS);
  }
}

#ifdef VERSION
void checkForUpdates(void * parameter){
  for(;;){
    firmwareUpdate();
    vTaskDelay((CHECK_FOR_UPDATES_INTERVAL*1000) / portTICK_PERIOD_MS);
  }
}

void firmwareUpdate(){
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
}
#else
void checkForOTA(void * parameter){
  for(;;){
    ArduinoOTA.handle();
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}
#endif

void checkStats(void * parameter){
  for(;;){
    log_d("Total heap: %d", ESP.getHeapSize());
    log_d("Free heap: %d", ESP.getFreeHeap());
    log_d("Total PSRAM: %d", ESP.getPsramSize());
    log_d("Free PSRAM: %d", ESP.getFreePsram());
    vTaskDelay(15000 / portTICK_PERIOD_MS);
  }
}