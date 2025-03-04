#include <Arduino.h>
#include "EventMsg.h"
#include "EventDispatcher.h"

EventMsg eventMsg;
EventDispatcher dispatcher("file_system", 0x01);  // Device name and local address

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize EventMsg
    eventMsg.init([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
    
    // Example handler showing header modification with device name
    dispatcher.on("ping", [](const char* deviceName, const char* eventName, const char* data, EventHeader& header) {
        Serial.printf("Device %s received %s from 0x%02X\n", deviceName, eventName, header.senderId);
        
        // Modify header for response
        header.groupId = 0x01;  // Change group for response
        header.flags = 0x01;    // Set custom flag
        
        // Create response header using modified header
        auto responseHeader = dispatcher.createResponseHeader(header);
        eventMsg.send("pong", "response", responseHeader);
    });
    
    // Handler demonstrating different response types
    dispatcher.on("query", [](const char* deviceName, const char* eventName, const char* data, EventHeader& header) {
        Serial.printf("Device %s handling %s: %s\n", deviceName, eventName, data);
        
        if (strcmp(data, "broadcast") == 0) {
            // Send broadcast response
            auto broadcastHeader = dispatcher.createHeader(0xFF);
            eventMsg.send("response", "broadcast_reply", broadcastHeader);
        }
        else if (strcmp(data, "group") == 0) {
            // Send group response
            auto groupHeader = dispatcher.createHeader(0xFF, 0x01);
            eventMsg.send("response", "group_reply", groupHeader);
        }
        else {
            // Send direct response
            auto responseHeader = dispatcher.createResponseHeader(header);
            eventMsg.send("response", "direct_reply", responseHeader);
        }
    });
    
    // Register dispatcher with EventMsg
    auto dispatcherHeader = dispatcher.createHeader(0x01);
    eventMsg.registerDispatcher("file_system", dispatcherHeader, dispatcher.getHandler());
    
    Serial.println("Callback Demo Ready!");
    Serial.println("Commands:");
    Serial.println("1. ping - Test ping/pong with header modification");
    Serial.println("2. query broadcast - Test broadcast response");
    Serial.println("3. query group - Test group response");
    Serial.println("4. query direct - Test direct response");
}

void loop() {
    // Process incoming serial data
    if (Serial.available()) {
        String cmd = Serial.readStringUntil(' ');
        String data = Serial.readStringUntil('\n');
        
        if (cmd == "ping") {
            auto header = dispatcher.createHeader(0x01);
            eventMsg.send("ping", "test", header);
        }
        else if (cmd == "query") {
            auto header = dispatcher.createHeader(0x01);
            eventMsg.send("query", data.c_str(), header);
        }
    }
    
    // Process incoming messages
    while (Serial.available()) {
        uint8_t byte = Serial.read();
        eventMsg.process(&byte, 1);
    }
}
