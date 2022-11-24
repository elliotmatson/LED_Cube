#ifndef CUBE_H
#define CUBE_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#include <FastLED.h>
#include <Preferences.h>
#include <HomeSpan.h>
//#include <WebSerial.h>

#include "config.h"

extern uint8_t const cos_wave[256];
extern TaskHandle_t checkForUpdatesTask;
extern TaskHandle_t checkForOTATask;
extern MatrixPanel_I2S_DMA *dma_display;
extern VirtualMatrixPanel *virtualDisp;
extern Preferences prefs;

void initStorage();
void initUpdates();
void initDisplay();
void initWifi();
void showDebug();
inline uint8_t fastCosineCalc(uint16_t preWrapVal);
inline float projCalcX(uint8_t x, uint8_t y);
inline uint8_t projCalcIntX(uint8_t x, uint8_t y);
inline uint8_t projCalcY(uint8_t x, uint8_t y);
void firmwareUpdate();
void checkForUpdates(void *parameter);

struct DEV_Cube : Service::LightBulb {       // Dimmable LED

  SpanCharacteristic *power;                        // reference to the On Characteristic
  SpanCharacteristic *level;                        // NEW! Create a reference to the Brightness Characteristic instantiated below
  
  DEV_Cube() : Service::LightBulb(){       // constructor() method

    power=new Characteristic::On();     
                
    level=new Characteristic::Brightness(50);       // NEW! Instantiate the Brightness Characteristic with an initial value of 50% (same as we did in Example 4)
    level->setRange(0,100,1);                       // NEW! This sets the range of the Brightness to be from a min of 5%, to a max of 100%, in steps of 1% (different from Example 4 values)
    
  } // end constructor

  boolean update(){                              // update() method

    // Here we set the brightness of the LED by calling ledPin->set(brightness), where brightness=0-100.
    // Note HomeKit sets the on/off status of a LightBulb separately from its brightness, which means HomeKit
    // can request a LightBulb be turned off, but still retains the brightness level so that it does not need
    // to be resent once the LightBulb is turned back on.
    
    // Multiplying the newValue of the On Characteristic ("power", which is a boolean) with the newValue of the
    // Brightness Characteristic ("level", which is an integer) is a short-hand way of creating the logic to
    // set the LED level to zero when the LightBulb is off, or to the current brightness level when it is on.
    
    dma_display->setBrightness8(map(power->getNewVal()*level->getNewVal(),0,100,0,255));
    return(true);                               // return true
  
  } 
};

#endif