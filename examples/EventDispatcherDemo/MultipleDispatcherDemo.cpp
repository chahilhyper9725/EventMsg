#include <Arduino.h>
#include "EventMsg.h"
#include "EventDispatcher.h"

// Define device addresses
#define DEVICEBROADCAST 0xFF
#define DEVICE01 0x01

EventMsg eventMsg;

// Create multiple dispatchers for different subsystems
EventDispatcher fileDispatcher(DEVICE01);    // File operations
EventDispatcher sensorDispatcher(DEVICE01);  // Sensor handling
EventDispatcher networkDispatcher(DEVICE01); // Network operations

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize EventMsg
    eventMsg.init([](uint8_t* data, size_t len) {
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
    
    // Send startup notifications
    {
        auto header = fileDispatcher.createHeader(DEVICEBROADCAST);
        eventMsg.send("subsystem_ready", "file_handler", header);
    }
    {
        auto header = sensorDispatcher.createHeader(DEVICEBROADCAST);
        eventMsg.send("subsystem_ready", "sensor_handler", header);
    }
    {
        auto header = networkDispatcher.createHeader(DEVICEBROADCAST);
        eventMsg.send("subsystem_ready", "network_handler", header);
    }
    
    Serial.println("Multiple dispatcher demo ready!");
    Serial.println("Available commands:");
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
            
            // Choose appropriate dispatcher based on command type
            if (command == "deleteFile" || command == "renameFile") {
                auto header = fileDispatcher.createHeader(DEVICE01);
                eventMsg.send(command.c_str(), data.c_str(), header);
            }
            else if (command == "readTemp" || command == "readHumidity") {
                auto header = sensorDispatcher.createHeader(DEVICE01);
                eventMsg.send(command.c_str(), "request", header);
            }
            else if (command == "forward" || command == "ping") {
                auto header = networkDispatcher.createHeader(DEVICEBROADCAST);
                eventMsg.send(command.c_str(), data.c_str(), header);
            }
        }
        else {
            // Process as protocol data
            eventMsg.process((uint8_t*)&c, 1);
        }
    }
    
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
