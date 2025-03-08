#include <Arduino.h>
#include "EventMsg.h"
#include "EventDispatcher.h"

// Create instances
EventMsg eventMsg;
uint8_t serialSourceId;  // Source ID for serial input
EventDispatcher fileDispatcher(0x01);  // Initialize with local address

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Create serial source with appropriate buffer size
    serialSourceId = eventMsg.createSource(256, 8);
    
    // Set write callback separately
    eventMsg.setWriteCallback([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
    
    // Register file operation handlers
    fileDispatcher.on("deleteFile", [](const char* data, EventHeader& header) {
        Serial.printf("Deleting file: %s\n", data);
        
        // Create response header automatically
        auto responseHeader = fileDispatcher.createResponseHeader(header);
        eventMsg.send("fileDeleted", "success", responseHeader);
    });
    
    fileDispatcher.on("renameFile", [](const char* data, EventHeader& header) {
        Serial.printf("Renaming file: %s\n", data);
        
        // Create response header automatically
        auto responseHeader = fileDispatcher.createResponseHeader(header);
        eventMsg.send("fileRenamed", "success", responseHeader);
    });
    
    fileDispatcher.on("listFiles", [](const char* data, EventHeader& header) {
        Serial.println("Listing files in directory");
        
        // Example directory listing response
        const char* fileList = "file1.txt,file2.txt,data.bin";
        
        // Create response header automatically
        auto responseHeader = fileDispatcher.createResponseHeader(header);
        eventMsg.send("fileList", fileList, responseHeader);
    });
    
    // Register dispatcher with EventMsg using helper
    eventMsg.registerDispatcher("fileHandler", 
                              fileDispatcher.createHeader(0x01),  // Only handle direct messages
                              fileDispatcher.getHandler());
    
    Serial.println("File operation handler ready!");
    Serial.printf("Serial source created with ID: %d\n", serialSourceId);
    Serial.println("Available commands:");
    Serial.println("1. deleteFile <filename>");
    Serial.println("2. renameFile <oldname>:<newname>");
    Serial.println("3. listFiles <directory>");
    
    // Send startup notification
    auto header = fileDispatcher.createHeader(0xFF); // Broadcast
    eventMsg.send("fileHandler", "ready", header);
}

void loop() {
    // Process any incoming serial data
    while (Serial.available()) {
        uint8_t byte = Serial.read();
        sourceManager.pushToSource(serialSourceId, &byte, 1);
    }
    
    // Process all sources
    eventMsg.processAllSources();
    
    // Send status update every 10 seconds
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus >= 10000) {
        char status[32];
        snprintf(status, sizeof(status), "Uptime: %lus", millis() / 1000);
        
        // Create broadcast header for status update
        auto header = fileDispatcher.createHeader(0xFF);
        eventMsg.send("fileStatus", status, header);
        
        lastStatus = millis();
    }
}
