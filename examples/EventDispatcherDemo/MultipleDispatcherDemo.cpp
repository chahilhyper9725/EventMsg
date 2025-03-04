#include <Arduino.h>
#include "EventMsg.h"
#include "EventDispatcher.h"

EventMsg eventMsg;

// Create multiple dispatchers for different subsystems
EventDispatcherInfo fileDispatcher(DEVICE01);    // File operations
EventDispatcherInfo sensorDispatcher(DEVICE01);  // Sensor handling
EventDispatcherInfo networkDispatcher(DEVICE01); // Network operations

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize EventMsg
    eventMsg.init([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
    
    // Set up file operations
    fileDispatcher.on("deleteFile", [](const char* data, const EventHeader& header) {
        Serial.printf("Deleting file: %s\n", data);
        auto responseHeader = fileDispatcher.createResponseHeader(header);
        eventMsg.send("fileDeleted", "success", responseHeader);
    });
    
    fileDispatcher.on("renameFile", [](const char* data, const EventHeader& header) {
        Serial.printf("Renaming file: %s\n", data);
        auto responseHeader = fileDispatcher.createResponseHeader(header);
        eventMsg.send("fileRenamed", "success", responseHeader);
    });
    
    // Set up sensor operations
    sensorDispatcher.on("readTemp", [](const char* data, const EventHeader& header) {
        float temp = 25.5; // Example temperature reading
        char response[32];
        snprintf(response, sizeof(response), "%.1f", temp);
        auto responseHeader = sensorDispatcher.createResponseHeader(header);
        eventMsg.send("tempData", response, responseHeader);
    });
    
    sensorDispatcher.on("readHumidity", [](const char* data, const EventHeader& header) {
        float humidity = 60.0; // Example humidity reading
        char response[32];
        snprintf(response, sizeof(response), "%.1f", humidity);
        auto responseHeader = sensorDispatcher.createResponseHeader(header);
        eventMsg.send("humidityData", response, responseHeader);
    });
    
    // Set up network operations
    networkDispatcher.on("forward", [](const char* data, const EventHeader& header) {
        Serial.printf("Forwarding message: %s\n", data);
        // Create broadcast header
        auto broadcastHeader = networkDispatcher.createHeader(DEVICEBROADCAST);
        eventMsg.send("message", data, broadcastHeader);
    });
    
    networkDispatcher.on("ping", [](const char* data, const EventHeader& header) {
        auto responseHeader = networkDispatcher.createResponseHeader(header);
        eventMsg.send("pong", "alive", responseHeader);
    });
    
    // Register each dispatcher with EventMsg
    auto deviceHeader = fileDispatcher.createHeader(DEVICE01);
    auto sensorHeader = sensorDispatcher.createHeader(DEVICE01);
    auto networkHeader = networkDispatcher.createHeader(DEVICEBROADCAST);
    
    eventMsg.registerDispatcher("file_handler", deviceHeader, fileDispatcher.getHandler());
    eventMsg.registerDispatcher("sensor_handler", sensorHeader, sensorDispatcher.getHandler());
    eventMsg.registerDispatcher("network_handler", networkHeader, networkDispatcher.getHandler());
    
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
            
            // Choose appropriate dispatcher based on command
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
    }
}
