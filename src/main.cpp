#include "config.h"
#include "cube.h"
#include "patterns.h"
//#include "secrets.h"


void setup()
{
  Serial.begin(115200);
  initPrefs();
  initDisplay();
  initWifi();
  initUpdates();
  showDebug();

  delay(5000);
}

void loop()
{
  plasma();
}