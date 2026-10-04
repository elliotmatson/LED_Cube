#include "cube.h"

Cube ledCube; // not "cube": that is the geometry namespace (lib/cube_geometry)

// Tells the Arduino core not to confirm a freshly updated image as soon as it
// boots. Cube::init() confirms it instead, once the cube is actually working,
// so an image that resets during startup is rolled back.
//
// extern "C": the core declares its weak default in C (esp32-hal-misc.c).
// Without it this was a separate, C++-named function the core never called,
// so every update was confirmed the moment it started.
extern "C" bool verifyRollbackLater()
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
