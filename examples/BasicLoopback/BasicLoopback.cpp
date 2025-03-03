#include <Arduino.h>
#include "EventMsg.h"

EventMsg eventMsg;

// Handler for loopback messages
void loopbackHandler(const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.println("\n=== Loopback Message ===");
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X\n", sender, receiver);
    
    // Echo message back with a different event name
    char responseData[64];
    snprintf(responseData, sizeof(responseData), "Echo: %s", data);
    eventMsg.send("ECHO_RESPONSE", responseData, sender, 0x00, 0x00);
}

// Handler for echo responses
void echoHandler(const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.println("\n=== Echo Response ===");
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X\n", sender, receiver);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("EventMsg Basic Loopback Demo with Dispatchers");
    Serial.println("Send messages in format: t<eventname> <data>");
    Serial.println("Example: tTEST Hello\n");
    
    // Initialize with Serial write callback
    eventMsg.init([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
    
    // Set local address for this device
    eventMsg.setAddr(0x01);
    
    // Register dispatchers for different message types
    eventMsg.registerDispatcher("loopback", 0xFF, 0x00, loopbackHandler);  // Handle all incoming messages
    eventMsg.registerDispatcher("echo", 0x01, 0x00, echoHandler);         // Handle responses to our device
}

void loop() {
    // Process any incoming serial data
    while (Serial.available()) {
        char c = Serial.read();
        if (c == 't') {  // Command format: t<eventname> <data>
            String eventname = Serial.readStringUntil(' ');
            String eventdata = Serial.readStringUntil('\n');
            
            // Send as broadcast to demonstrate loopback
            eventMsg.send(eventname.c_str(), eventdata.c_str(), 0xFF, 0x00, 0x00);
        }
        else {
            // Process as regular message data
            eventMsg.process((uint8_t*)&c, 1);
        }
    }
}
