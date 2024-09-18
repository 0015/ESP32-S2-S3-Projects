# ESP32-S3 UVC & UAC Test with WebSocket Server

This project demonstrates capturing images and audio data from a USB camera and microphone connected to the ESP32-S3 via UVC (USB Video Class) and UAC (USB Audio Class). The data is then streamed using a WebSocket server for testing purposes.

## Project Structure
- **WebSocketServer**: Node.js WebSocket server to receive and display raw image and audio data from the ESP32-S3.
- **uvc_uac_test**: ESP32-S3 firmware that captures image and audio data from USB devices and sends it to the WebSocket server.

---

## Features
- Captures video using **UVC** and audio using **UAC** on the ESP32-S3.
- Streams raw image and audio data over WebSocket for testing.
- Logs packet index and data sizes in the WebSocket client for detailed testing.
- The WebSocket server is designed for testing purposes; to transmit images and audio simultaneously for real-world applications, use a protocol like **RTSP**.

## Hardware Requirements
- ESP32-S3 board with USB OTG support.
- USB Camera compatible with UVC.
- USB Microphone compatible with UAC.
- USB OTG cable to connect peripherals to the ESP32-S3.

---

## Getting Started

### 1. WebSocketServer Setup

1. Navigate to the `WebSocketServer` directory.
2. Install dependencies:
   ```JSON
   {
    "dependencies": {
        "express": "^4.21.0",
        "path": "^0.12.7",
        "ws": "^8.18.0"
        }
    }
   ```
   ```bash
   npm install ws express path
   ```
3. Start the WebSocket server:
   ```bash
   node server.js
   ```
   The server will listen for incoming connections on `ws://localhost:8080` (or the IP address of your server).

### 2. ESP32-S3 Firmware Setup

1. Flash the ESP32-S3 with the **uvc_uac_test** firmware:
   - Open the `uvc_uac_test` folder.
   - Update WiFi credentials in WiFi Settings(menuconfig)
   - Build and flash the project using ESP-IDF:
     ```bash
     idf.py build
     idf.py flash monitor
     ```
2. Connect the USB camera(Built-in MIC) to the ESP32-S3 via the USB OTG port or GPIO 19/20.

---

## Usage

1. Once both the server and ESP32-S3 are set up, the ESP32-S3 will start streaming image and audio data to the WebSocket server.
2. Open the **WebSocket client** (HTML) on a browser to visualize incoming image and audio packets. You can toggle image/audio reception and adjust the audio volume.
3. The client logs raw data packet indices and sizes for debugging purposes.

---

## Limitations

- **WebSocket is for Testing Only**: This project uses WebSocket to transmit raw data for testing purposes. To properly stream video and audio simultaneously in a real-world application, a protocol like **RTSP** should be used.
- The ESP32-S3 may not support high-frame-rate or high-resolution video streams due to hardware limitations.

---

## Future Improvements

- Optimize audio and video packet synchronization.
- Add multi-client support for streaming.
