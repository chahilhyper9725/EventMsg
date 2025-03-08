#include <Arduino.h>
#include "EventMsg.h"
#include "EventDispatcher.h"

// Define device addresses
#define DEVICEBROADCAST 0xFF
#define DEVICE01 0x01

EventMsg eventMsg;

// Source IDs for different subsystems
uint8_t serialSourceId;   // For serial input
uint8_t fileSourceId;     // For file operations
uint8_t sensorSourceId;   // For sensor data
uint8_t networkSourceId;  // For network operations

// Create multiple dispatchers for different subsystems
EventDispatcher fileDispatcher(DEVICE01);    // File operations
EventDispatcher sensorDispatcher(DEVICE01);  // Sensor handling
EventDispatcher networkDispatcher(DEVICE01); // Network operations

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Create sources with appropriate buffer sizes
    serialSourceId = eventMsg.createSource(256, 8);     // Serial input
    fileSourceId = eventMsg.createSource(1024, 8);      // Large for file ops
    sensorSourceId = eventMsg.createSource(64, 4);      // Small for sensor data
    networkSourceId = eventMsg.createSource(512, 16);   // Medium for network
    
    // Set write callback separately
    eventMsg.setWriteCallback([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
    
    // Set up file operations
    fileDispatcher.on("deleteFile", [](const char* data, EventHeader& header) {
        Serial.printf("Deleting file: %s\n", data);
        auto responseHeader = fileDispatcher.createResponseHeader(header);
        eventMsg.send("fileDeleted", "success", responseHeader);
    });
    
    fileDispatcher.on("renameFile", [](const char* data, EventHeader& header) {
        Serial.printf("Renaming file: %s\n", data);
        auto responseHeader = fileDispatcher.createResponseHeader(header);
        eventMsg.send("fileRenamed", "success", responseHeader);
    });
    
    // Set up sensor operations
    sensorDispatcher.on("readTemp", [](const char* data, EventHeader& header) {
        float temp = 25.5; // Example temperature reading
        char response[32];
        snprintf(response, sizeof(response), "%.1f", temp);
        auto responseHeader = sensorDispatcher.createResponseHeader(header);
        eventMsg.send("tempData", response, responseHeader);
    });
    
    sensorDispatcher.on("readHumidity", [](const char* data, EventHeader& header) {
        float humidity = 60.0; // Example humidity reading
        char response[32];
        snprintf(response, sizeof(response), "%.1f", humidity);
        auto responseHeader = sensorDispatcher.createResponseHeader(header);
        eventMsg.send("humidityData", response, responseHeader);
    });
    
    // Set up network operations
    networkDispatcher.on("forward", [](const char* data, EventHeader& header) {
        Serial.printf("Forwarding message: %s\n", data);
        // Create broadcast header
        auto broadcastHeader = networkDispatcher.createHeader(DEVICEBROADCAST);
        eventMsg.send("message", data, broadcastHeader);
    });
    
    networkDispatcher.on("ping", [](const char* data, EventHeader& header) {
        auto responseHeader = networkDispatcher.createResponseHeader(header);
        eventMsg.send("pong", "alive", responseHeader);
    });
    
    // Register each dispatcher with EventMsg using the simplified API
    eventMsg.registerDispatcher("file_handler", 
                              fileDispatcher.createHeader(DEVICE01), 
                              fileDispatcher.getHandler());
                              
    eventMsg.registerDispatcher("sensor_handler", 
                              sensorDispatcher.createHeader(DEVICE01), 
                              sensorDispatcher.getHandler());
                              
    eventMsg.registerDispatcher("network_handler", 
                              networkDispatcher.createHeader(DEVICEBROADCAST), 
                              networkDispatcher.getHandler());
    
    // Print source configuration
    Serial.println("Multiple dispatcher demo ready!");
    Serial.printf("Serial source ID: %d (256B, 8 slots)\n", serialSourceId);
    Serial.printf("File source ID: %d (1KB, 8 slots)\n", fileSourceId);
    Serial.printf("Sensor source ID: %d (64B, 4 slots)\n", sensorSourceId);
    Serial.printf("Network source ID: %d (512B, 16 slots)\n", networkSourceId);
    
    Serial.println("\nAvailable commands:");
    Serial.println("1. deleteFile <filename>");
    Serial.println("2. renameFile <oldname:newname>");
    Serial.println("3. readTemp");
    Serial.println("4. readHumidity");
    Serial.println("5. forward <message>");
    Serial.println("6. ping");
}

void loop() {
    // Process incoming serial data
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (c == 't') {  // Test commands
            String command = Serial.readStringUntil(' ');
            String data = Serial.readStringUntil('\n');
            
            // Queue data to appropriate source
            uint8_t sourceId;
            
            if (command == "deleteFile" || command == "renameFile") {
                sourceId = fileSourceId;
            }
            else if (command == "readTemp" || command == "readHumidity") {
                sourceId = sensorSourceId;
            }
            else {
                sourceId = networkSourceId;
            }
            
            // Create command packet
            char packet[256];
            snprintf(packet, sizeof(packet), "%s:%s", command.c_str(), data.c_str());
            sourceManager.pushToSource(sourceId, (uint8_t*)packet, strlen(packet));
        }
        else {
            sourceManager.pushToSource(serialSourceId, (uint8_t*)&c, 1);
        }
    }
    
    // Process all sources
    eventMsg.processAllSources();
    
    // Send periodic status updates from each subsystem
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus >= 10000) {
        char status[32];
        snprintf(status, sizeof(status), "Uptime: %lus", millis() / 1000);
        
        // Send status from each subsystem
        {
            auto header = fileDispatcher.createHeader(DEVICEBROADCAST);
            eventMsg.send("file_status", status, header);
        }
        {
            auto header = sensorDispatcher.createHeader(DEVICEBROADCAST);
            eventMsg.send("sensor_status", status, header);
        }
        {
            auto header = networkDispatcher.createHeader(DEVICEBROADCAST);
            eventMsg.send("network_status", status, header);
        }
        
        lastStatus = millis();
    }
}
