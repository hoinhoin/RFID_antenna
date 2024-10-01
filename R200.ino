#include "R200.h"
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <WiFiAP.h>
#include <NewPing.h>

// Ultrasound sensor for activate
#define HCSRIN 13
#define HCSROUT 12
#define MAX_DIS 200
#define DOORDIS 10
#define BUTTONPIN 18
NewPing sonar(HCSRIN, HCSROUT, MAX_DIS);

// Set these to your desired credentials.
const char *ssid = "yourAP";
const char *password = "yourPassword";

WebSocketsServer webSocketServer(8080);
unsigned long lastResetTime = 0;
R200 rfid;

int tricket[3] = {0, 0, 0};  // Array to track ticket status
int num = 0x00;  // Variable to store the received hex value

// Define RFID values for trk1 to trk5 (initially set to 0)
int trkowner = 0x2C;
int trk1 = 0x63;
int trk2 = 0x84;
int numowner = 0;
int num1 = 0;
int num2 = 0;

#ifndef LED_BUILTIN
#define LED_BUILTIN 2  // Set the GPIO pin where you connected your test LED
#endif

// WebSocket event handler to process incoming data
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    if (type == WStype_CONNECTED) {
        Serial.printf("WebSocket client #%u connected\n", num);
    } else if (type == WStype_DISCONNECTED) {
        Serial.printf("WebSocket client #%u disconnected\n", num);
    } else if (type == WStype_TEXT) {
        // Process WebSocket text message to register RFID values for trk0 to trk5
        String receivedData = String((char*)payload);  // Convert payload to String
        Serial.printf("Received data from client: %s\n", receivedData.c_str());

        // Parse the incoming data, expecting messages like "SET TRK1 0x12345678"
        if (receivedData.startsWith("SET TRKOWNER ")) {
            trkowner = strtol(receivedData.substring(12).c_str(), NULL, 16);
            Serial.printf("Owner track set to: %X\n", trkowner);
        } else if (receivedData.startsWith("SET TRK1 ")) {
            trk1 = strtol(receivedData.substring(9).c_str(), NULL, 16);
            Serial.printf("Track 1 set to: %X\n", trk1);
        } else if (receivedData.startsWith("SET TRK2 ")) {
            trk2 = strtol(receivedData.substring(9).c_str(), NULL, 16);
            Serial.printf("Track 2 set to: %X\n", trk2);
        }
    }
}

// Function to check if trkowner is 1 and trk1 and trk3 are also set to 1

void distinguish() {
    if (tricket[0] == 1) {
      while(numowner == 0){
          Serial.println("Owner detected");
          webSocketServer.broadcastTXT("0000");
          numowner = 1;
          break;
        }
    }
    if (tricket[1] == 1) {
      while(num1 == 0){
          Serial.println("Track 1 detected");
          webSocketServer.broadcastTXT("1111");
          num1 = 1;
          break;
        }
    }
    if (tricket[2] == 1) {
      while(num2 == 0){
          Serial.println("Track 2 detected");
          webSocketServer.broadcastTXT("2222");
          num2 = 1;
          break;
        }
  }
}
void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(BUTTONPIN, INPUT);
    delay(1000);
    Serial.begin(115200);
    Serial.println(__FILE__ " " __DATE__);

    // Initialize RFID module
    rfid.begin(&Serial2, 115200, 16, 17);
    rfid.dumpModuleInfo();

    // Wi-Fi setup
    Serial.println("Configuring access point...");
    if (!WiFi.softAP(ssid, password)) {
        log_e("Soft AP creation failed.");
        while (1);  // halt if Wi-Fi setup fails
    }
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    // Initialize WebSocket server
    webSocketServer.begin();
    webSocketServer.onEvent(onWebSocketEvent);
    Serial.println("WebSocket server started");
}

void loop() {
    
    webSocketServer.loop();

    // 초음파 센서로 거리 측정: 일정 거리 이상일 때만 아래 코드가 실행됨
    while (sonar.ping_cm() >= DOORDIS) {
       

        // RFID and WebSocket server handling
        rfid.loop();
          // Poll RFID module every second
    if (millis() - lastResetTime > 1000) {
      rfid.poll();
      lastResetTime = millis();
    }

        // Send RFID UID to all connected clients
        String uidData = "RFID UID: ";
        for (int i = 0; i < 12; i++) {
            uidData += String(rfid.uid[i], HEX) + " ";
            if (i == 11) {
                num = rfid.uid[i];  // This is where the RFID value is set to `num`
            }
        }
        Serial.println(num, HEX);

        // Check if `num` matches the registered tracks
        if (num == trkowner) {
            tricket[0] = 1;
        } else if (num == trk1) {
            tricket[1] = 1;
        } else if (num == trk2) {
            tricket[2] = 1;
        }

 // Broadcast RFID UID to all clients
        Serial.println("RFID UID sent to all clients: " + uidData);

        distinguish();  // 기존의 distinguish 기능 유지

        // 트랙 소유자가 1일 때, 조건을 확인하고 알림을 보냄

        
    }
    if (digitalRead(BUTTONPIN) == HIGH) {

            tricket[0] = 0;
            delay(10);
            numowner = 0;
            tricket[1] = 0;
            delay(10);
            num1 = 0;
            tricket[2] = 0;
            delay(10);
            num2 = 0;
            Serial.println("reseted!");
            webSocketServer.broadcastTXT("Button pushed: reset");
        }

        delay(200);  // Delay to prevent flooding the network
}
