#include "config.h"
#include "cube.h"


unsigned long frameCount=25500;
void drawFrame(void * parameter);
TaskHandle_t drawFrameTask = NULL;


void setup() {
    Serial.begin(115200);
    Serial.printf("Booting: \n");
    if(psramInit()){
      Serial.println("PSRAM is correctly initialized");
    }else{
      Serial.println("PSRAM not available");
    }
    
    initWifi();
    initUpdates();
    initDisplay();

    showDebug();
    delay(5000);
}

void loop(){
  dma_display->fillScreenRGB888(255, 0, 0);
  delay(500);
   dma_display->fillScreenRGB888(0, 255, 0);
   delay(500);
   dma_display->fillScreenRGB888(0, 0, 255);
   delay(500);
   dma_display->fillScreenRGB888(255, 255, 255);
   delay(500);
    dma_display->fillScreenRGB888(0, 0, 0);

   for (uint8_t i = 0; i < 64*PANELS_NUMBER; i++) {
     for (uint8_t j = 0; j < 64; j++) {
       dma_display->drawPixelRGB888(i,j,255,255,255);
     }
     delay(50);
   }
   for (uint8_t i = 0; i < 64*PANELS_NUMBER; i++) {
     for (uint8_t j = 0; j < 64; j++) {
       dma_display->drawPixelRGB888(i,j,0,0,0);
     }
     delay(50);
   }
   for (uint8_t j = 0; j < 64; j++) {
     for (uint8_t i = 0; i < 64*PANELS_NUMBER; i++) {
       dma_display->drawPixelRGB888(i,j,255,255,255);
     }
     delay(50);
   }
   for (uint8_t j = 0; j < 64; j++) {
     for (uint8_t i = 0; i < 64*PANELS_NUMBER; i++) {
       dma_display->drawPixelRGB888(i,j,0,0,0);
     }
     delay(50);
   }
}

void plasma()
{
  frameCount++ ; 
  uint16_t t = fastCosineCalc((42 * frameCount)>>6);  //time displacement - fiddle with these til it looks good...
  uint16_t t2 = fastCosineCalc((35 * frameCount)>>6); 
  uint16_t t3 = fastCosineCalc((38 * frameCount)>>6);
        
  for (uint8_t i = 0; i < PANEL_WIDTH*PANELS_NUMBER; i++) {
    for (uint8_t j = 0; j < PANEL_HEIGHT ; j++) { 
      uint8_t x = (projCalcIntX(i,j)+66);
      uint8_t y = (projCalcY(i,j)+66);

      uint8_t r = fastCosineCalc(((x << 3) + (t >> 1) + fastCosineCalc((t2 + (y << 3))))>>2);
      uint8_t g = fastCosineCalc(((y << 3) + t + fastCosineCalc(((t3 >> 2) + (x << 3))))>>2);
      uint8_t b = fastCosineCalc(((y << 3) + t2 + fastCosineCalc((t + x + (g >> 2))))>>2);

      dma_display->drawPixelRGB888(i,j,r,g,b);
    }
  }
}


void drawFrame(void * parameter){
  for(;;){
    plasma();
    vTaskDelay(33 / portTICK_PERIOD_MS);
  }
}