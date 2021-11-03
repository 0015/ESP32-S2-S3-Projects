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
