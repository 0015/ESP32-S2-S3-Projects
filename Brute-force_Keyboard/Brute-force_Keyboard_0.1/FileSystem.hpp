#pragma once
#include "SPIFFS.h"

class FileSystem {
  private:
    const char* TARGET_FILE = "/passwords.txt";
    bool SPIFFSReady;
    bool fileReady;
    File targetFile;

  public:

    FileSystem();

    void format();
    void listSPIFFS();
    void readLine();
    bool isFileReady();
    int totalPW();
    String read();
};
