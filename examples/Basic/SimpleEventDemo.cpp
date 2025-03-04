#include <Arduino.h>
#include <EventMsg.h>

// Create EventMsg instance
EventMsg eventMsg;

// LED pin (built-in LED on most ESP32 boards)
const int LED_PIN = 2;

// Event dispatcher handler with device name
// Event handler for main functionality
void mainHandler(const char* deviceName, const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.printf("=== Event from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X\n", sender, receiver);

    if (strcmp(event, "LED_CONTROL") == 0) {
        bool state = (data[0] == '1');
        digitalWrite(LED_PIN, state);
        char response[32];
        snprintf(response, sizeof(response), "LED is now %s", state ? "ON" : "OFF");
        eventMsg.send("LED_STATUS", response, sender, 0x00, 0x00); // Reply to sender
    }
    else if (strcmp(event, "PING") == 0) {
        eventMsg.send("PONG", data, sender, 0x00, 0x00); // Reply to sender
    }
}

// Raw data handler for monitoring traffic
void monitorHandler(const char* deviceName, const char* event, uint8_t* data, size_t length) {
    Serial.printf("=== Monitor %s: %s ===\n", deviceName, event);
    Serial.printf("Length: %d bytes\n", length);
    Serial.print("Raw data: ");
    for(size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();
}

// Unhandled event handler
void unhandledHandler(const char* deviceName, const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.printf("=== Unhandled Event from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X\n", sender, receiver);
}

// Raw data handler
void rawHandler(const char* deviceName, const char* event, uint8_t* data, size_t length) {
    Serial.printf("=== Raw Data from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Length: %d bytes\n", length);
    Serial.print("Data (hex): ");
    for(size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();
}

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    // Configure LED pin
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // Initialize EventMsg with Serial write callback
    eventMsg.init([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
    
    // Set device address (1) and group (0)
    eventMsg.setAddr(0x01);
    eventMsg.setGroup(0x00);
    
    // Register handlers
    eventMsg.registerDispatcher("main", 0x01, 0x00, mainHandler);      // Direct messages
    eventMsg.registerDispatcher("broadcast", 0xFF, 0x00, mainHandler); // Broadcasts
    eventMsg.registerRawHandler("monitor", 0xFF, 0x00, monitorHandler); // Traffic monitoring
    eventMsg.setUnhandledHandler("unhandled", 0xFF, 0x00, unhandledHandler); // Unmatched events
    
    Serial.println("EventMsg Demo Ready!");
    Serial.println("Commands:");
    Serial.println("1. LED_CONTROL with data '1' or '0'");
    Serial.println("2. PING with any data");
}

void loop() {
    // Process incoming serial data
    while (Serial.available()) {
        uint8_t byte = Serial.read();
        eventMsg.process(&byte, 1);
    }
    
    // Send heartbeat every 5 seconds
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat >= 5000) {
        char data[32];
        snprintf(data, sizeof(data), "Uptime: %lus", millis() / 1000);
        eventMsg.send("HEARTBEAT", data, 0xFF, 0x00, 0x00);
        lastHeartbeat = millis();
    }
}
