/////////////////////////////////////////////////////////////////
/*
  Cracking password on Mac?? Brute-force USB Keyboard using ESP32-S2 (based on Keyset)
  Video Tutorial: https://youtu.be/AJ1lSk_aK6M
  Created by Eric N. (ThatProject)
*/
/////////////////////////////////////////////////////////////////


#include "MainProgram.hpp"
#include "USB.h"
#include "USBHIDKeyboard.h"
MainProgram *mainProg;

void setup() {
  Serial.begin(115200);
  
  USBHIDKeyboard Keyboard;
  Keyboard.begin();
  USB.begin();

  mainProg = new MainProgram(Keyboard);
  mainProg->begin();
}

void loop() {
  mainProg->loop();
  delay(10);
}
