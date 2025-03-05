#include <Arduino.h>
#include <EventMsg.h>

EventMsg eventMsg;

// Event dispatcher handler with new signature
void eventHandler(const char* deviceName, const char* event, const char* data, EventHeader& header) {
    Serial.printf("=== Event from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X, Group: 0x%02X\n", 
                 header.senderId, header.receiverId, header.groupId);
    
    // Create response header and echo back
    EventHeader responseHeader{
        0x01,            // our address as sender
        header.senderId, // respond to original sender
        0x00,           // no group
        0x00            // no flags
    };
    eventMsg.send(event, data, responseHeader);
}

// Raw data handler with simplified signature
void rawHandler(const char* deviceName, const uint8_t* data, size_t length) {
    Serial.printf("=== Raw Data from %s ===\n", deviceName);
    Serial.print("Length: ");
    Serial.println(length);
    Serial.print("Data (hex): ");
    for(size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();
}

// Unhandled event handler with event dispatcher signature
void unhandledHandler(const char* deviceName, const char* event, const char* data, EventHeader& header) {
    Serial.printf("=== Unhandled Event from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X, Group: 0x%02X\n",
                 header.senderId, header.receiverId, header.groupId);
    Serial.println("Message was not processed by any dispatcher");
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    // Initialize with Serial write callback
    eventMsg.init([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
    
    // Set device address
    eventMsg.setAddr(0x01);
    eventMsg.setGroup(0x00);
    
    // Create default headers for registering handlers
    EventHeader directHeader{0x00, 0x01, 0x00, 0x00};  // For direct messages to us
    EventHeader broadcastHeader{0x00, 0xFF, 0x00, 0x00}; // For broadcast messages
    
    // Register handlers with new callback signatures
    eventMsg.registerDispatcher("loopback", directHeader, eventHandler);
    eventMsg.registerDispatcher("broadcast", broadcastHeader, eventHandler);
    
    // Register raw handler for monitoring messages
    eventMsg.registerRawHandler("monitor", broadcastHeader, rawHandler);
    
    // Set handler for unmatched events
    eventMsg.setUnhandledHandler("unhandled", broadcastHeader, unhandledHandler);
    
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
            
            // Send as broadcast
            EventHeader header{0x01, 0xFF, 0x00, 0x00};
            eventMsg.send(eventname.c_str(), eventdata.c_str(), header);
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
        
        EventHeader header{0x01, 0xFF, 0x00, 0x00}; // Broadcast heartbeat
        eventMsg.send("HEARTBEAT", data, header);
        
        lastHeartbeat = millis();
    }
}
