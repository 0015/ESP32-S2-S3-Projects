#include "MainProgram.hpp"
#define INPUT_SPEED 1000 //ms
static MainProgram *instance = NULL;

MainProgram::MainProgram() {}

MainProgram::MainProgram(USBHIDKeyboard _keyboard) {
  totalPW = 0;
  indexPW = 0;
  timer = 0;
  currentState = NONE;
  instance = this;
  keyboard = _keyboard;
}

MainProgram::~MainProgram() {}

void displayCallbackEvent() {
  instance->currentState = START;
}

void MainProgram::begin() {
  void (*ptr)(void) = &displayCallbackEvent;
  display = new Display(ptr);
  fileSystem = new FileSystem();
  if (fileSystem->isFileReady()) {
    totalPW = fileSystem->totalPW();
    display->printTwoString("Password SET", String(totalPW));
    delay(1500);
    if(totalPW != 0){
      display->countdownScreen();
    }
    
  } else {
    display->printString("File Error");
    currentState = ERROR;
  }
}

void MainProgram::loop() {

  switch (currentState) {
    case START:
      if (millis() - timer >= INPUT_SPEED) {
        timer = millis();

        if (indexPW >= totalPW) {
          currentState = END;
          return;
        }
        
        String pwString = fileSystem->read();
        if (pwString.length() != 0) {
          keyboard.println(pwString);
          display->updateScreen(indexPW++, totalPW, pwString);
          delay(500);
          keyboard.println();
        }
      }
      break;

    case END:
      delay(10000);
      display->printString("EOF");
      break;

    default: break;
  }

}
