#include "config.h"
#include "cube.h"
#include "patterns.h"



void setup()
{
  Serial.begin(115200);
  Serial.printf("Booting: \n");

  initPsram();
  initWifi();
  initUpdates();
  initDisplay();
  showDebug();

  delay(5000);
}

void loop()
{
  dma_display->fillScreenRGB888(0, 0, 255);
}