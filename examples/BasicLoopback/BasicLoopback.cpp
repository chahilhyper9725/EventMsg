#include <Arduino.h>
#include <EventMsg.h>

EventMsg eventMsg;

// Event dispatcher handler
void eventHandler(const char* deviceName, const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.printf("=== Event from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X\n", sender, receiver);
    
    // Echo back the received message
    eventMsg.send(event, data, sender, 0x00, 0x00);
}

// Raw data handler - shows all messages in hex
void rawHandler(const char* deviceName, const char* event, uint8_t* data, size_t length) {
    Serial.printf("=== Raw Data from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Length: %d bytes\n", length);
    Serial.print("Data (hex): ");
    for(size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();
}

// Unhandled event handler with same signature as dispatchers
void unhandledHandler(const char* deviceName, const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.printf("=== Unhandled Event from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X\n", sender, receiver);
    Serial.println("Message was not processed by any dispatcher");
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    // Initialize with Serial write callback
    eventMsg.init([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
    
    // Set device address and group
    eventMsg.setAddr(0x01);
    eventMsg.setGroup(0x00);
    
    // Register handlers
    eventMsg.registerDispatcher("loopback", 0x01, 0x00, eventHandler);  // Handle direct messages
    eventMsg.registerDispatcher("broadcast", 0xFF, 0x00, eventHandler); // Handle broadcasts
    
    // Register raw data handler for monitoring all messages
    eventMsg.registerRawHandler("monitor", 0xFF, 0x00, rawHandler);
    
    // Set handler for unmatched events
    eventMsg.setUnhandledHandler("unhandled", 0xFF, 0x00, unhandledHandler);
    
    Serial.println("EventMsg Loopback Demo Ready!");
    Serial.println("Enter: t[eventname] [eventdata] to send test messages");
}

void loop() {
    // Process any incoming data
    while (Serial.available()) {
        char c = Serial.read();
        if (c == 't') {  // t[eventname] [eventdata]
            String eventname = Serial.readStringUntil(' ');
            String eventdata = Serial.readStringUntil('\n');
            // Send as broadcast to demonstrate
            eventMsg.send(eventname.c_str(), eventdata.c_str(), 0xFF, 0x00, 0x00);
        }
        else {
            // Process as protocol data
            eventMsg.process((uint8_t*)&c, 1);
        }
    }
    
    // Send heartbeat every 5 seconds
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat >= 5000) {
        char data[32];
        snprintf(data, sizeof(data), "Uptime: %lus", millis() / 1000);
        eventMsg.send("HEARTBEAT", data, 0xFF, 0x00, 0x00);
        lastHeartbeat = millis();
    }
}
