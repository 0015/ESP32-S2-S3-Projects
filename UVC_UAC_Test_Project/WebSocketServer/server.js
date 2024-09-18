/* Websocket server for UVC, UAC testing
 *
 * [How to use it]
 * 1. Check you local IP first. 
 * (e.g.) 192.168.50.80
 * 2. Change local IP in client.html line #222 
 * (e.g.) ws://192.168.50.80:8080
 * 3. Access http://localhost:8000/client or http://192.168.50.80:8000/client in a web browser.
 */

const path = require("path");
const express = require("express");
const WebSocket = require("ws");
const app = express();

const WS_PORT = process.env.WS_PORT || 8080;
const HTTP_PORT = process.env.HTTP_PORT || 8000;

const wsServer = new WebSocket.Server({ port: WS_PORT }, () =>
  console.log(`WS server is listening at ws://localhost:${WS_PORT}`)
);

const imageQueue = [];
const audioQueue = [];
const IMAGE_DELAY = 1000;  // 1 second delay for images to sync with audio

// array of connected websocket clients
let connectedClients = [];
let esp32Client = null;
let enabledAudio = false;
let enabledImage = false;

wsServer.on("connection", (ws, req) => {
  
    // Check the protocol from the initial handshake to identify the client
    const clientProtocol = req.headers['sec-websocket-protocol'];
    if (clientProtocol === 'ESP32') {
      console.log("ESP32 client connected");
      esp32Client = ws;  // Store the ESP32 client separately
    } else {
      console.log("HTML client connected");
      connectedClients.push(ws);  // Add HTML clients to the array
    }

    ws.on("message", (data) => {
      if (esp32Client && ws === esp32Client) {
        if (data instanceof Buffer) {
          if (data.length === 512) {
            // Sending without syncing
            if(enabledAudio && !enabledImage){
              broadcastData('audio', data);
            }else {
              // Add audio data to audioQueue
              audioQueue.push(data);
    
              // Send synchronized audio and images
              sendSyncedData();
            }
          } else if (isValidJPEG(data)) {  
            // Add image data to imageQueue with a delay timestamp
            imageQueue.push({ data, timestamp: Date.now() });
  
            // Process the image queue after a delay to sync with audio
            setTimeout(() => {
              processImageQueue();
            }, IMAGE_DELAY);
          }
        }
      }else{
         if (typeof data === "string") {
          
          const settings = JSON.parse(data);

          if(settings.command === "toggleAudio"){
            enabledAudio = settings.enabled;
          }else if(settings.command === "imageCheckbox"){
            enabledImage = settings.enabled;
          }
          
          if(esp32Client != null){
            esp32Client.send(data);
          }
        }

      }
    });
  
    ws.on("close", () => {
      console.log("Client disconnected");
      connectedClients = connectedClients.filter(client => client !== ws);
      if (ws === esp32Client) {
        esp32Client = null;  // Clear ESP32 client on disconnection
      }
    });
  });

// Helper function to validate if the Buffer is a valid JPEG image
function isValidJPEG(data) {
  // JPEG files start with 0xFFD8 and end with 0xFFD9
  const SOI = [0xFF, 0xD8];  // Start of image marker
  const EOI = [0xFF, 0xD9];  // End of image marker

  return (
    data.length > 2 &&
    data[0] === SOI[0] && data[1] === SOI[1] &&  // Check for SOI at the start
    data[data.length - 2] === EOI[0] && data[data.length - 1] === EOI[1]  // Check for EOI at the end
  );
}  

// Function to process the image queue and send images to clients after delay
function processImageQueue() {
  const now = Date.now();
  while (imageQueue.length > 0) {
    const { data, timestamp } = imageQueue[0];
    if (now - timestamp >= IMAGE_DELAY) {
      // Send image data to all connected HTML clients
      broadcastData('image', data);
      imageQueue.shift();  // Remove the processed image from the queue
    } else {
      break;  // Stop if we haven't reached the delay time
    }
  }
}

// Function to send synchronized audio and image data
function sendSyncedData() {
  // Check if both queues have data to send
  if (audioQueue.length > 0 && imageQueue.length > 0) {
    // Send audio data
    broadcastData('audio', audioQueue.shift());

    // We already process images in processImageQueue()
  }
}

// Helper function to broadcast data to HTML clients
function broadcastData(type, data) {
  connectedClients.forEach((client, i) => {
    if (client.readyState === client.OPEN) {
      // First, send a message indicating the type (audio or image)
      client.send(JSON.stringify({
        type: type,
        length: data.length  // Include the length of the data
      }));

      // Then, send the actual binary data
      client.send(data);
    } else {
      connectedClients.splice(i, 1);  // Remove disconnected clients
    }
  });
}

// HTTP stuff
app.get("/client", (req, res) =>
  res.sendFile(path.resolve(__dirname, "./client.html"))
);
app.listen(HTTP_PORT, () =>
  console.log(`HTTP server listening at http://localhost:${HTTP_PORT}`)
);