#include <Arduino.h>
#include "EventMsg.h"
#include "EventDispatcher.h"

EventMsg eventMsg;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize EventMsg
    eventMsg.init([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
    
    // Set local address
    eventMsg.setAddr(0x01);
    
    // Register raw data handler for monitoring
    auto rawHeader = EventHeader{0x00, 0xFF, 0x00, 0x00};  // Broadcast monitoring
    eventMsg.registerRawHandler("monitor", rawHeader, 
        [](const char* deviceName, const char* eventName, const uint8_t* data, size_t length, EventHeader& header) {
            Serial.printf("=== Raw Data from %s ===\n", deviceName);
            Serial.printf("Event: %s\n", eventName);
            Serial.printf("Length: %d bytes\n", length);
            Serial.printf("From: 0x%02X, To: 0x%02X, Group: 0x%02X\n", 
                        header.senderId, header.receiverId, header.groupId);
            
            // Print data in hex format
            Serial.print("Data (hex): ");
            for(size_t i = 0; i < length; i++) {
                Serial.printf("%02X ", data[i]);
            }
            Serial.println();
        });
    
    // Register unhandled event handler
    eventMsg.setUnhandledHandler("unhandled", rawHeader,
        [](const char* deviceName, const char* eventName, const char* data, EventHeader& header) {
            Serial.printf("=== Unhandled Event from %s ===\n", deviceName);
            Serial.printf("Event: %s\n", eventName);
            Serial.printf("Data: %s\n", data);
            Serial.printf("From: 0x%02X, To: 0x%02X, Group: 0x%02X\n",
                        header.senderId, header.receiverId, header.groupId);
        });
    
    // Create a dispatcher for testing
    EventDispatcher testDispatcher("test_device", 0x01);
    
    // Register test event handler
    testDispatcher.on("test", [](const char* deviceName, const char* eventName, const char* data, EventHeader& header) {
        Serial.printf("=== Test Event from %s ===\n", deviceName);
        Serial.printf("Event: %s\n", eventName);
        Serial.printf("Data: %s\n", data);
        
        // Send response
        auto responseHeader = EventDispatcher::createResponseHeader(header);
        eventMsg.send("test_response", "received", responseHeader);
    });
    
    // Register dispatcher
    auto dispatcherHeader = testDispatcher.createHeader(0x01);
    eventMsg.registerDispatcher("test_device", dispatcherHeader, testDispatcher.getHandler());
    
    Serial.println("Raw Handler Demo Ready!");
    Serial.println("Commands:");
    Serial.println("1. test <data> - Send test event");
    Serial.println("2. raw <data> - Send raw data");
    Serial.println("3. unhandled <data> - Send unhandled event");
}

void loop() {
    // Process incoming serial data
    if (Serial.available()) {
        String cmd = Serial.readStringUntil(' ');
        String data = Serial.readStringUntil('\n');
        
        auto header = EventHeader{0x01, 0x01, 0x00, 0x00};
        
        if (cmd == "test") {
            eventMsg.send("test", data.c_str(), header);
        }
        else if (cmd == "raw") {
            eventMsg.send("raw_data", data.c_str(), header);
        }
        else if (cmd == "unhandled") {
            eventMsg.send("unknown_event", data.c_str(), header);
        }
    }
    
    // Process incoming messages
    while (Serial.available()) {
        uint8_t byte = Serial.read();
        eventMsg.process(&byte, 1);
    }
}
