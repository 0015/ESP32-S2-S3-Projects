#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
    lgfx::Light_PWM _light_instance;

  public:
    LGFX(void) {
      {
        auto cfg = _bus_instance.config();
        cfg.spi_host = SPI2_HOST;
        cfg.spi_mode = 3;
        cfg.freq_write = 20000000;
        cfg.freq_read = 16000000;
        cfg.spi_3wire = true;
        cfg.use_lock = true;
        cfg.dma_channel = 1;
        cfg.pin_sclk = 36;
        cfg.pin_mosi = 35;
        cfg.pin_miso = 4;
        cfg.pin_dc = 37;
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
      }

      {
        auto cfg = _panel_instance.config();
        cfg.pin_cs = 34;
        cfg.pin_rst = 38;
        cfg.pin_busy = -1;
        cfg.memory_width = 135;
        cfg.memory_height = 240;
        cfg.panel_width = 135;
        cfg.panel_height = 240;
        cfg.offset_x = 203;
        cfg.offset_y = 40;
        cfg.offset_rotation = 0;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits = 1;
        cfg.readable = true;
        cfg.invert = true;
        cfg.rgb_order = false;
        cfg.dlen_16bit = false;
        cfg.bus_shared = false;

        _panel_instance.config(cfg);
      }

      {
        auto cfg = _light_instance.config();
        cfg.pin_bl = 33;
        cfg.invert = false;
        cfg.freq = 44100;
        cfg.pwm_channel = 7;

        _light_instance.config(cfg);
        _panel_instance.setLight(&_light_instance);
      }

      setPanel(&_panel_instance);
    };
};

class Display {

  private:
    LGFX tft;
    void drawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t percent, uint16_t frameColor, uint16_t barColor);
    typedef void (*FuncPtrVoid)(void);
    FuncPtrVoid cdCallback;
  public:
    Display();
    Display(FuncPtrVoid f);
    void printString(String msg);
    void printTwoString(String title, String msg);
    void updateScreen(int pos, int end, String key);
    void countdownScreen();

};
