#include <Arduino.h>
#include <EventMsg.h>
#include <EventDispatcher.h>

// Create EventMsg instance
EventMsg eventMsg;

// Source ID for serial data
uint8_t serialSourceId;

// Create dispatchers for different purposes
EventDispatcher mainDispatcher(0x01);    // Local address 0x01
EventDispatcher monitorDispatcher(0x01); // For monitoring
EventDispatcher unhandledDispatcher(0x01); // For unhandled events

// LED pin (built-in LED on most ESP32 boards)
const int LED_PIN = 2;

// Raw data handler for monitoring traffic
void monitorCallback(const char* data, EventHeader& header) {
    Serial.printf("=== Monitor Traffic ===\n");
    Serial.printf("From: 0x%02X, To: 0x%02X, Group: 0x%02X\n", 
                 header.senderId, header.receiverId, header.groupId);
    Serial.printf("Data: %s\n", data);
}

// Setup event handlers using the dispatcher interface
void setupHandlers() {
    // LED control handler
    mainDispatcher.on("LED_CONTROL", [](const char* data, EventHeader& header) {
        bool state = (data[0] == '1');
        digitalWrite(LED_PIN, state);
        
        auto responseHeader = mainDispatcher.createResponseHeader(header);
        char response[32];
        snprintf(response, sizeof(response), "LED is now %s", state ? "ON" : "OFF");
        eventMsg.send("LED_STATUS", response, responseHeader);
    });
    
    // Ping handler
    mainDispatcher.on("PING", [](const char* data, EventHeader& header) {
        auto responseHeader = mainDispatcher.createResponseHeader(header);
        eventMsg.send("PONG", data, responseHeader);
    });
    
    // Monitor handler
    monitorDispatcher.on("*", monitorCallback);
    
    // Unhandled events handler
    unhandledDispatcher.on("*", [](const char* data, EventHeader& header) {
        Serial.printf("=== Unhandled Event ===\n");
        Serial.printf("From: 0x%02X, To: 0x%02X, Group: 0x%02X\n", 
                     header.senderId, header.receiverId, header.groupId);
        Serial.printf("Data: %s\n", data);
    });
}

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    // Configure LED pin
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Create serial source with appropriate buffer size
    serialSourceId = eventMsg.createSource(256, 8);
    Serial.printf("Created serial source (ID: %d) with 256B buffer\n", serialSourceId);
    
    // Set write callback separately
    eventMsg.setWriteCallback([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
    
    // Set up handlers
    setupHandlers();
    
    // Register dispatchers with EventMsg
    eventMsg.registerDispatcher("main", 
                              mainDispatcher.createHeader(0x01), // Direct messages
                              mainDispatcher.getHandler());
                              
    eventMsg.registerDispatcher("monitor", 
                              monitorDispatcher.createHeader(0xFF), // Monitor all messages
                              monitorDispatcher.getHandler());
                              
    // Set unhandled event handler
    eventMsg.setUnhandledHandler("unhandled", 
                                unhandledDispatcher.createHeader(0xFF),
                                unhandledDispatcher.getHandler());
    
    Serial.println("EventMsg Demo Ready!");
    Serial.println("Commands:");
    Serial.println("1. LED_CONTROL with data '1' or '0'");
    Serial.println("2. PING with any data");
}

void loop() {
    // Queue incoming serial data
    while (Serial.available()) {
        uint8_t byte = Serial.read();
        sourceManager.pushToSource(serialSourceId, &byte, 1);
    }

    // Process all sources
    eventMsg.processAllSources();
    
    // Send heartbeat every 5 seconds
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat >= 5000) {
        char data[32];
        snprintf(data, sizeof(data), "Uptime: %lus", millis() / 1000);
        
        // Create broadcast header for heartbeat
        auto header = mainDispatcher.createHeader(0xFF); // Broadcast address
        eventMsg.send("HEARTBEAT", data, header);
        
        lastHeartbeat = millis();
    }
}
