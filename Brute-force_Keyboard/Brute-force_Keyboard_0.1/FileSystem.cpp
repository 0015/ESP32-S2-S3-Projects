#include "FileSystem.hpp"

FileSystem::FileSystem() {
  SPIFFSReady = SPIFFS.begin();
}

bool FileSystem::isFileReady() {

  if (!SPIFFSReady) return SPIFFSReady;


  targetFile = SPIFFS.open(TARGET_FILE, "r");
  return !targetFile ? false : true;
}

int FileSystem::totalPW() {
  int cnt = 0;
  char buffer[64];
  while (targetFile.available()) {
    targetFile.readBytesUntil('\n', buffer, sizeof(buffer));
    cnt++;
  }

  targetFile.seek(0, SeekSet);
  return cnt;
}

String FileSystem::read() {
  char buffer[64];
  if (targetFile.available()) {
    int l = targetFile.readBytesUntil('\n', buffer, sizeof(buffer));
    buffer[l] = 0;
    return String(buffer);
  } else {
    return String("");
  }

}

void FileSystem::readLine() {
  targetFile = SPIFFS.open("/passwords.txt", "r");

  char buffer[64];
  while (targetFile.available()) {
    int l = targetFile.readBytesUntil('\n', buffer, sizeof(buffer));
    buffer[l] = 0;
    Serial.println(buffer);
  }

  targetFile.close();
}
