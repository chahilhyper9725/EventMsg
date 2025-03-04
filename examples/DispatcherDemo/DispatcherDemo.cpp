#include <Arduino.h>
#include "EventMsg.h"

EventMsg eventMsg;

// Callback handler for BLE phone device
void phoneHandler(const char* deviceName, const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.printf("=== Phone Event from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X\n", sender, receiver);
    
    if (strcmp(event, "ble_connect") == 0) {
        Serial.println("Processing BLE connection request");
    }
    else if (strcmp(event, "send_lua") == 0) {
        Serial.println("Receiving Lua code via BLE");
    }
}

// Callback handler for ESP32-NOW nodes
void nodeHandler(const char* deviceName, const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.printf("=== Node Event from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X\n", sender, receiver);
    
    if (strcmp(event, "espnow_forward") == 0) {
        Serial.println("Forwarding message to ESP-NOW network");
    }
    else if (strcmp(event, "broadcast_cmd") == 0) {
        Serial.println("Broadcasting command to all nodes");
    }
}

// Raw data handler for monitoring
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

// Unhandled event handler (same signature as event dispatchers)
void unhandledHandler(const char* deviceName, const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.printf("=== Unhandled Event from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X\n", sender, receiver);
    Serial.println("Message was not processed by any dispatcher");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize event message system with serial write callback
    eventMsg.init([](uint8_t* data, size_t len) {
        Serial.write(data, len);
        return true;
    });
    
    // Set local address for this device
    eventMsg.setAddr(0x01);
    
    // Register dispatchers for different device types
    eventMsg.registerDispatcher("phone1", 0x01, 0x00, phoneHandler);   // Phone on addr 0x01
    eventMsg.registerDispatcher("nodes", 0xFF, 0x00, nodeHandler);     // All ESP32 nodes
    eventMsg.registerDispatcher("group1", 0xFF, 0x01, nodeHandler);    // Group 1 nodes
    
    // Register raw data handler for monitoring
    eventMsg.registerRawHandler("monitor", 0xFF, 0x00, rawHandler);    // Monitor all messages
    
    // Set handler for unmatched events
    eventMsg.setUnhandledHandler("unhandled", 0xFF, 0x00, unhandledHandler);
    
    // Test sending messages
    Serial.println("Sending test messages...\n");
    
    // Direct message to phone
    eventMsg.send("ble_connect", "request_conn", 0x01, 0x00, 0x00);
    delay(100);
    
    // Broadcast to all nodes
    eventMsg.send("broadcast_cmd", "status_request", 0xFF, 0x00, 0x00);
    delay(100);
    
    // Message to Group 1
    eventMsg.send("group_msg", "hello group 1", 0xFF, 0x01, 0x00);
}

void loop() {
    // Process any incoming serial data
    while (Serial.available()) {
        uint8_t data = Serial.read();
        eventMsg.process(&data, 1);
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
