#include <Arduino.h>
#include "EventMsg.h"
#include "EventDispatcher.h"

// Create instances
EventMsg eventMsg;
EventDispatcher fileDispatcher;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize EventMsg
    eventMsg.init([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
    
    // Set addresses
    eventMsg.setAddr(0x01);
    eventMsg.setGroup(0x00);
    
    // Register file operation handlers
    fileDispatcher.on("deleteFile", [](const char* data, const EventHeader& header) {
        Serial.printf("Deleting file: %s\n", data);
        
        // Send response back to sender
        EventHeader responseHeader = {
            0x01,           // Our address
            header.senderId, // Original sender
            0x00,           // No group
            0x00            // No flags
        };
        eventMsg.send("fileDeleted", "success", responseHeader);
    });
    
    fileDispatcher.on("renameFile", [](const char* data, const EventHeader& header) {
        Serial.printf("Renaming file: %s\n", data);
        
        // Send response back
        EventHeader responseHeader = {0x01, header.senderId, 0x00, 0x00};
        eventMsg.send("fileRenamed", "success", responseHeader);
    });
    
    // Register dispatcher with EventMsg
    EventHeader dispatcherHeader = {
        0x00,    // senderId not used for registration
        0x01,    // Only handle messages sent to us
        0x00,    // No group filtering
        0x00     // No flags
    };
    eventMsg.registerDispatcher("fileHandler", dispatcherHeader, fileDispatcher.getHandler());
    
    
    Serial.println("File operation handler ready!");
    Serial.println("Available commands:");
    Serial.println("1. deleteFile <filename>");
    Serial.println("2. renameFile <oldname>:<newname>");
}

void loop() {
    // Process any incoming serial data
    while (Serial.available()) {
        uint8_t byte = Serial.read();
        eventMsg.process(&byte, 1);
    }
}
