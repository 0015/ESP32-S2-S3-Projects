#include <Arduino.h>
#include "Display.hpp"
#include "FileSystem.hpp"
#include "USBHIDKeyboard.h"

typedef enum {
  NONE,
  START,
  END,
  ERROR
} State_t;

class MainProgram {
  private:
    Display *display;
    FileSystem *fileSystem;

    int totalPW;
    int indexPW;
    long timer;

    USBHIDKeyboard keyboard;

  public:
    State_t currentState;
    MainProgram();
    MainProgram(USBHIDKeyboard _keyboard);
    ~MainProgram();
    void begin();
    void loop();
};
