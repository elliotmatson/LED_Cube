#include "cube.h"
//#include "secrets.h"

Cube cube(true);

void setup()
{
  cube.init();
}


// Just do nothing, eveything is done in tasks
void loop()
{
  yield();
  vTaskDelay(100/portTICK_PERIOD_MS);
}
