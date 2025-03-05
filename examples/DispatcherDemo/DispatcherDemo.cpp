#include <Arduino.h>
#include "EventMsg.h"
#include "EventDispatcher.h"

EventMsg eventMsg;

// Create dispatchers for different device types
EventDispatcher phoneDispatcher(0x01);    // For phone communication
EventDispatcher nodeDispatcher(0x01);     // For ESP32 node communication
EventDispatcher monitorDispatcher(0x01);  // For traffic monitoring
EventDispatcher unhandledDispatcher(0x01); // For unhandled events

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize event message system with serial write callback
    eventMsg.init([](uint8_t* data, size_t len) {
        Serial.write(data, len);
        return true;
    });
    
    // Set up phone communication handlers
    phoneDispatcher.on("ble_connect", [](const char* data, EventHeader& header) {
        Serial.println("Processing BLE connection request");
        
        // Send response using helper
        auto responseHeader = phoneDispatcher.createResponseHeader(header);
        eventMsg.send("ble_status", "connected", responseHeader);
    });
    
    phoneDispatcher.on("send_lua", [](const char* data, EventHeader& header) {
        Serial.println("Receiving Lua code via BLE");
        Serial.printf("Code: %s\n", data);
        
        auto responseHeader = phoneDispatcher.createResponseHeader(header);
        eventMsg.send("lua_status", "received", responseHeader);
    });
    
    // Set up ESP32 node handlers
    nodeDispatcher.on("espnow_forward", [](const char* data, EventHeader& header) {
        Serial.println("Forwarding message to ESP-NOW network");
        
        // Create broadcast header for forwarding
        auto broadcastHeader = nodeDispatcher.createHeader(0xFF);
        eventMsg.send("forward", data, broadcastHeader);
    });
    
    nodeDispatcher.on("broadcast_cmd", [](const char* data, EventHeader& header) {
        Serial.println("Broadcasting command to all nodes");
        Serial.printf("Command: %s\n", data);
        
        // Send acknowledgment back to sender
        auto responseHeader = nodeDispatcher.createResponseHeader(header);
        eventMsg.send("cmd_ack", "received", responseHeader);
    });
    
    // Set up monitoring for all traffic
    monitorDispatcher.on("*", [](const char* data, EventHeader& header) {
        Serial.printf("=== Monitor Traffic ===\n");
        Serial.printf("From: 0x%02X, To: 0x%02X, Group: 0x%02X\n", 
                     header.senderId, header.receiverId, header.groupId);
        Serial.printf("Data: %s\n", data);
    });
    
    // Set up unhandled event handler
    unhandledDispatcher.on("*", [](const char* data, EventHeader& header) {
        Serial.printf("=== Unhandled Event ===\n");
        Serial.printf("From: 0x%02X, To: 0x%02X, Group: 0x%02X\n", 
                     header.senderId, header.receiverId, header.groupId);
        Serial.printf("Data: %s\n", data);
        Serial.println("Message was not processed by any dispatcher");
    });
    
    // Register dispatchers with EventMsg using proper headers
    eventMsg.registerDispatcher("phone1", 
                              phoneDispatcher.createHeader(0x01),  // Phone messages
                              phoneDispatcher.getHandler());
                              
    eventMsg.registerDispatcher("nodes", 
                              nodeDispatcher.createHeader(0xFF),   // All nodes
                              nodeDispatcher.getHandler());
                              
    eventMsg.registerDispatcher("group1", 
                              nodeDispatcher.createHeader(0xFF, 0x01),  // Group 1
                              nodeDispatcher.getHandler());
                              
    eventMsg.registerDispatcher("monitor", 
                              monitorDispatcher.createHeader(0xFF),  // All traffic
                              monitorDispatcher.getHandler());
                              
    // Set handler for unmatched events
    eventMsg.setUnhandledHandler("unhandled", 
                                unhandledDispatcher.createHeader(0xFF),
                                unhandledDispatcher.getHandler());
    
    // Test sending messages
    Serial.println("Sending test messages...\n");
    
    // Direct message to phone
    {
        auto header = phoneDispatcher.createHeader(0x01);
        eventMsg.send("ble_connect", "request_conn", header);
    }
    delay(100);
    
    // Broadcast to all nodes
    {
        auto header = nodeDispatcher.createHeader(0xFF);
        eventMsg.send("broadcast_cmd", "status_request", header);
    }
    delay(100);
    
    // Message to Group 1
    {
        auto header = nodeDispatcher.createHeader(0xFF, 0x01);
        eventMsg.send("group_msg", "hello group 1", header);
    }
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
        
        // Use broadcast header for heartbeat
        auto header = phoneDispatcher.createHeader(0xFF);
        eventMsg.send("HEARTBEAT", data, header);
        
        lastHeartbeat = millis();
    }
}
