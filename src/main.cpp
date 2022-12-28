#include "config.h"
#include "cube.h"
#include "patterns.h"
//#include "secrets.h"
#include "snakes.h"

SnakeGame game(10,20);

void setup()
{
  Serial.begin(115200);
  initPrefs();
  initDisplay();
  initWifi();
  initUpdates();
  showDebug();

  delay(5000);
  game.init_game(); 
}

void loop()
{
  game.update();
  game.draw();
}