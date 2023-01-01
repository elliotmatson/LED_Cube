//#include "secrets.h"
#include "cube.h"

Cube cube;

void setup()
{
  cube.init();
}


// Just do nothing, eveything is done in tasks
void loop()
{
  vTaskDelete(NULL); // Delete Loop task, we don't need it
}
