#if __has_include("secrets.h")
#include "secrets.h"
#endif
#include "cube.h"

Cube cube;

void setup()
{
  // Immediately pulls display enable pin low to keep panels from flickering on boot
  pinMode(OE_PIN, OUTPUT);
  digitalWrite(OE_PIN, LOW);
  cube.init();
}


// Just do nothing, eveything is done in tasks
void loop()
{
  vTaskDelete(NULL); // Delete Loop task, we don't need it
}
