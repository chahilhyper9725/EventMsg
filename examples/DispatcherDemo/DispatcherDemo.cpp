#include <Arduino.h>
#include "EventMsg.h"

EventMsg eventMsg;

// Callback handler for BLE phone device
void phoneHandler(const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.printf("Phone Handler - Event: %s, Data: %s, From: 0x%02X\n", event, data, sender);
    
    if (strcmp(event, "ble_connect") == 0) {
        Serial.println("Processing BLE connection request");
    }
    else if (strcmp(event, "send_lua") == 0) {
        Serial.println("Receiving Lua code via BLE");
    }
}

// Callback handler for ESP32-NOW nodes
void nodeHandler(const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.printf("Node Handler - Event: %s, Data: %s, From: 0x%02X\n", event, data, sender);
    
    if (strcmp(event, "espnow_forward") == 0) {
        Serial.println("Forwarding message to ESP-NOW network");
    }
    else if (strcmp(event, "broadcast_cmd") == 0) {
        Serial.println("Broadcasting command to all nodes");
    }
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
    eventMsg.registerDispatcher("phone1", 0x01, 0x00, phoneHandler);  // Specific phone
    eventMsg.registerDispatcher("esp32_nodes", 0xFF, 0x00, nodeHandler);  // All ESP32 nodes
    
    // Test sending messages
    Serial.println("Sending test messages...");
    
    // Send BLE connection request to specific phone
    eventMsg.send("ble_connect", "request_conn", 0x01, 0x00, 0x00);
    
    // Broadcast command to all ESP32 nodes
    eventMsg.send("broadcast_cmd", "status_request", 0xFF, 0x00, 0x00);
}

void loop() {
    // Process any incoming serial data
    while (Serial.available()) {
        uint8_t data = Serial.read();
        eventMsg.process(&data, 1);
    }
}
