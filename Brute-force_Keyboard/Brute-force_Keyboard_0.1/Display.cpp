#include <Arduino.h>
#include "Display.hpp"

Display::Display() {
}

Display::Display(FuncPtrVoid f) {
  cdCallback = f;
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setFont(&fonts::Orbitron_Light_24);
}

void Display::printString(String msg) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2);
}

void Display::printTwoString(String title, String msg) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(title, tft.width() / 2, tft.height() / 2 - 20);
  tft.drawString(msg, tft.width() / 2, tft.height() / 2 + 10);
}

void Display::drawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t percent, uint16_t frameColor, uint16_t barColor) {
  if (percent == 0) {
    tft.fillRoundRect(x, y, w, h, 3, TFT_BLACK);
  }
  uint8_t margin = 2;
  uint16_t barHeight = h - 2 * margin;
  uint16_t barWidth = w - 2 * margin;
  tft.drawRoundRect(x, y, w, h, 3, frameColor);
  tft.fillRect(x + margin, y + margin, barWidth * percent / 100.0, barHeight, barColor);
}

void Display::updateScreen(int pos, int end, String key) {
  if (pos == 0) {
    tft.setFont(&fonts::Orbitron_Light_24);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("[Brute-force]", 0, 0);
    tft.setTextDatum(MC_DATUM);
  }
  tft.setTextPadding(tft.width());
  tft.drawString(key, tft.width() / 2, 60);
  int percent = pos * 100 / end;
  this->drawProgressBar(20, tft.height() / 2 + 30, tft.width() - 40, 20, percent, TFT_RED, TFT_YELLOW);
}

void Display::countdownScreen() {

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);

  tft.setTextDatum(MC_DATUM);
  tft.drawString("[Brute-force]", tft.width() / 2, 20);
  tft.drawString("USB KEYBOARD", tft.width() / 2, 50);
  tft.drawString("Countdown", tft.width() / 2, 80);

  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextPadding(tft.width());
  for (int i = 3; i > -1; --i) {
    tft.drawString(String(i), tft.width() / 2, 110);
    delay(1000);
  }

  cdCallback();
}
