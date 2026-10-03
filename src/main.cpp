#include "cube.h"

Cube ledCube; // not "cube": that is the geometry namespace (lib/cube_geometry)

// Tells the Arduino core not to confirm a freshly updated image as soon as it
// boots. Cube::init() confirms it instead, once the cube is actually working,
// so an image that crashes during startup is rolled back.
bool verifyRollbackLater()
{
  return true;
}

void setup()
{
  // Immediately pulls display enable pin low to keep panels from flickering on boot
  pinMode(OE_PIN, OUTPUT);
  digitalWrite(OE_PIN, LOW);
  ledCube.init();
}


// Just do nothing, eveything is done in tasks
void loop()
{
  vTaskDelete(NULL); // Delete Loop task, we don't need it
}
