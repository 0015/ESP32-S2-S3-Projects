/////////////////////////////////////////////////////////////////
/*
  ESP32-S2 UVC Stream to LCD
  For More Information: https://youtu.be/lAVtS-HdMpo
  Created by Eric N. (ThatProject)
*/
/////////////////////////////////////////////////////////////////

// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "soc/efuse_reg.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "spi_bus.h"
#include "uvc_stream.h"
#include "jpegd2.h"

#define LGFX_USE_V1
#include "LovyanGFX.h"

#ifdef CONFIG_USE_PSRAM
#if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
#include "esp32/spiram.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/spiram.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/spiram.h"
#else
#error Target CONFIG_IDF_TARGET is not supported
#endif
#endif


/* USB PIN fixed in esp32-s2, can not use io matrix */
#define BOARD_USB_DP_PIN 20
#define BOARD_USB_DN_PIN 19

    class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9488 _panel_instance;
    lgfx::Bus_Parallel16 _bus_instance;
    lgfx::Light_PWM _light_instance;

public:
    LGFX(void)
    {
        {                                     
            auto cfg = _bus_instance.config();

            // 16位设置
            cfg.i2s_port = I2S_NUM_0;  
            cfg.freq_write = 80000000; 
            cfg.pin_wr = 35;           
            cfg.pin_rd = 34;           
            cfg.pin_rs = 36;           

            cfg.pin_d0 = 33;
            cfg.pin_d1 = 21;
            cfg.pin_d2 = 14;
            cfg.pin_d3 = 13;
            cfg.pin_d4 = 12;
            cfg.pin_d5 = 11;
            cfg.pin_d6 = 10;
            cfg.pin_d7 = 9;
            cfg.pin_d8 = 3;
            cfg.pin_d9 = 8;
            cfg.pin_d10 = 16;
            cfg.pin_d11 = 15;
            cfg.pin_d12 = 7;
            cfg.pin_d13 = 6;
            cfg.pin_d14 = 5;
            cfg.pin_d15 = 4;

            _bus_instance.config(cfg);              
            _panel_instance.setBus(&_bus_instance); 
        }

        {                                        
            auto cfg = _panel_instance.config(); 

            cfg.pin_cs = -1;   
            cfg.pin_rst = -1;  
            cfg.pin_busy = -1; 

            cfg.memory_width = 320;   
            cfg.memory_height = 480;  
            cfg.panel_width = 320;    
            cfg.panel_height = 480;   
            cfg.offset_x = 0;         
            cfg.offset_y = 0;         
            cfg.offset_rotation = 0;  
            cfg.dummy_read_pixel = 8; 
            cfg.dummy_read_bits = 1;  
            cfg.readable = true;      
            cfg.invert = false;       
            cfg.rgb_order = false;    
            cfg.dlen_16bit = true;    
            cfg.bus_shared = true;  

            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 45;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

static LGFX tft;


#define DESCRIPTOR_CONFIGURATION_INDEX 1
#define DESCRIPTOR_FORMAT_MJPEG_INDEX 1
#define DEMO_FRAME_WIDTH 320
#define DEMO_FRAME_HEIGHT 240
#define DEMO_XFER_BUFFER_SIZE (50 * 1024) // Double buffer
#define DEMO_FRAME_INDEX 4                // 320x240
#define DEMO_FRAME_INTERVAL 666666
#define DESCRIPTOR_STREAM_INTERFACE_INDEX 1
#define DESCRIPTOR_STREAM_ISOC_ENDPOINT_ADDR 0x81
#define DEMO_ISOC_EP_MPS 192
#define DEMO_ISOC_INTERFACE_ALT 1


#define DEMO_SPI_MAX_TRANFER_SIZE (CONFIG_LCD_BUF_WIDTH * CONFIG_LCD_BUF_HIGHT * 2 + 64)

static const char *TAG = "uvc_demo";


static void *_malloc(size_t size)
{
#ifdef CONFIG_USE_PSRAM
#ifndef CONFIG_ESP32S2_SPIRAM_SUPPORT
#error CONFIG_SPIRAM no defined, Please enable "Component config → ESP32S2-specific → Support for external SPI ram"
#endif
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
}

extern "C"
{
    void app_main(void);
}


static bool lcd_write_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data)
{
    tft.pushImage(x, y + 50, w, h, (uint16_t *)data);
    return true;
}

/* *******************************************************************************************
 * This callback function runs once per frame. Use it to perform any
 * quick processing you need, or have it put the frame into your application's
 * input queue. If this function takes too long, you'll start losing frames. */

int64_t current_ticks, delta_ticks;
int16_t fps = 0;

int64_t get_timestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}

static void frame_cb(uvc_frame_t *frame, void *ptr)
{
    assert(ptr);

    current_ticks = get_timestamp();
    uint8_t *lcd_buffer = (uint8_t *)(ptr);
    ESP_LOGE(TAG, "callback! frame_format = %d, seq = %u, width = %d, height = %d, length = %u, ptr = %d",
             frame->frame_format, frame->sequence, frame->width, frame->height, frame->data_bytes, (int) ptr);

    switch (frame->frame_format) {
        case UVC_FRAME_FORMAT_MJPEG:
            mjpegdraw((uint8_t *)frame->data, frame->data_bytes, lcd_buffer, lcd_write_bitmap);
            vTaskDelay(1 / portTICK_PERIOD_MS); /* add delay to free cpu to other tasks */
            break;

        default:
            ESP_LOGW(TAG, "Format not supported");
            assert(0);
            break;
    }

    delta_ticks = get_timestamp() - current_ticks;

    if (delta_ticks > 0)
        fps = 1000 / delta_ticks;

    tft.setCursor(tft.width() / 2 - 30, tft.height() - 50);
    tft.printf("FPS: %d ", fps);
}

void app_main(void)
{
    tft.init();
    tft.clear(TFT_WHITE);
    tft.setBrightness(255);
    tft.setRotation(2);
    tft.setFreeFont(&Orbitron_Light_24);
    tft.setTextColor(0x0000FFU, 0xFFFFFF);
    tft.drawString("UVC Stream To LCD", 10, 10);
    tft.setTextColor(0x000000U, 0xFFFFFF);
    tft.drawString("Resolution: 320x240", 10, tft.height()/2 + 60);
    tft.drawString("Camera: USB 2.0", 10, tft.height() / 2 + 90);
    tft.drawString("ESP32-S2", 10, tft.height() / 2 + 120);
    tft.drawString("USB 1.1: 12Mbps", 10, tft.height() / 2 + 150);
    tft.setFont(&FreeSansBoldOblique24pt7b);
    tft.setTextColor(0xFF0000U, 0xFFFFFF);
    //tft.setTextDatum(bottom_left);

    /* malloc a buffer for RGB565 data, as 320*240*2 = 153600B,
    here malloc a smaller buffer refresh lcd with steps */
    uint8_t *lcd_buffer = (uint8_t *)heap_caps_malloc(DEMO_SPI_MAX_TRANFER_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(lcd_buffer != NULL);

    /* malloc double buffer for usb payload, xfer_buffer_size >= frame_buffer_size*/
    uint8_t *xfer_buffer_a = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    assert(xfer_buffer_a != NULL);
    uint8_t *xfer_buffer_b = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    assert(xfer_buffer_b != NULL);

    /* malloc frame buffer for a jpeg frame*/
    uint8_t *frame_buffer = (uint8_t *)_malloc(DEMO_XFER_BUFFER_SIZE);
    assert(frame_buffer != NULL);

    /* the quick demo skip the standred get descriptors process,
    users need to get params from camera descriptors from PC side,
    eg. run `lsusb -v` in linux, then modify related MACROS */
    uvc_config_t uvc_config = {
        .dev_speed = USB_SPEED_FULL,
        .configuration = DESCRIPTOR_CONFIGURATION_INDEX,
        .format_index = DESCRIPTOR_FORMAT_MJPEG_INDEX,
        .frame_width = DEMO_FRAME_WIDTH,
        .frame_height = DEMO_FRAME_HEIGHT,
        .frame_index = DEMO_FRAME_INDEX,
        .frame_interval = DEMO_FRAME_INTERVAL,
        .interface = DESCRIPTOR_STREAM_INTERFACE_INDEX,
        .interface_alt = DEMO_ISOC_INTERFACE_ALT,
        .isoc_ep_addr = DESCRIPTOR_STREAM_ISOC_ENDPOINT_ADDR,
        .isoc_ep_mps = DEMO_ISOC_EP_MPS,
        .xfer_buffer_size = DEMO_XFER_BUFFER_SIZE,
        .xfer_buffer_a = xfer_buffer_a,
        .xfer_buffer_b = xfer_buffer_b,
        .frame_buffer_size = DEMO_XFER_BUFFER_SIZE,
        .frame_buffer = frame_buffer,
    };

    /* pre-config UVC driver with params from known USB Camera Descriptors*/
    esp_err_t ret = uvc_streaming_config(&uvc_config);

    /* Start camera IN stream with pre-configs, uvc driver will create multi-tasks internal
    to handle usb data from different pipes, and user's callback will be called after new frame ready. */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uvc streaming config failed");
    } else {
        uvc_streaming_start(frame_cb, (void *)(lcd_buffer));
    }

    while (1) {
        /* task monitor code if necessary */
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

}
