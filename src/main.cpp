#include "config.h"
#include "cube.h"
//#include "secrets.h"

<<<<<<< HEAD
SnakeGame game(100,10, 100);
TaskHandle_t showPatternTask;
void showPattern(void *parameter);
=======
Cube cube;
>>>>>>> 96efb4d1eec9f2637df363b097c824f470bf18c3

void setup()
{
  cube.init();
}

void loop()
{
  yield();
  vTaskDelay(100/portTICK_PERIOD_MS);
}
