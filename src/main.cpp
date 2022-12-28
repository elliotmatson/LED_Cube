#include "config.h"
#include "cube.h"
#include "patterns.h"
//#include "secrets.h"
#include "snakes.h"

SnakeGame game(100,10, 100);
TaskHandle_t showPatternTask;
void showPattern(void *parameter);

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
  showCoordinates();

  /*xTaskCreate(
      showPattern,     // Function that should be called
      "Show Pattern", // Name of the task (for debugging)
      8000,            // Stack size (bytes)
      NULL,            // Parameter to pass
      0,               // Task priority
      &showPatternTask // Task handle
  );*/
}

void loop()
{
  game.update();
  game.draw();
  yield();
}

/* void showPattern(void *parameter)
{
  while (true) {
    plasma();
  }
}*/
