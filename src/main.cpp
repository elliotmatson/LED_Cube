#include "config.h"
#include "cube.h"
//#include "secrets.h"

Cube cube(true);

void setup()
{
  cube.init();
}

void loop()
{
  yield();
  vTaskDelay(100/portTICK_PERIOD_MS);
}
