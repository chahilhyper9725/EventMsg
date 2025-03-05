#include <Arduino.h>
#include <EventMsg.h>
#include <EventDispatcher.h>

EventMsg eventMsg;

// Create dispatchers for different purposes
EventDispatcher mainDispatcher(0x01);     // For main message handling
EventDispatcher monitorDispatcher(0x01);  // For monitoring messages
EventDispatcher unhandledDispatcher(0x01); // For unhandled messages

// Setup event handlers using the dispatcher interface
void setupHandlers() {
    // Main event handler - echoes back all received messages
    mainDispatcher.on("*", [](const char* data, EventHeader& header) {
        Serial.printf("=== Event Handler ===\n");
        Serial.printf("Data: %s\n", data);
        Serial.printf("From: 0x%02X, To: 0x%02X, Group: 0x%02X\n", 
                     header.senderId, header.receiverId, header.groupId);
        
        // Create response header automatically
        auto responseHeader = mainDispatcher.createResponseHeader(header);
        eventMsg.send("ECHO", data, responseHeader);
    });
    
    // Monitor handler - logs all message traffic
    monitorDispatcher.on("*", [](const char* data, EventHeader& header) {
        Serial.printf("=== Monitor Traffic ===\n");
        Serial.printf("From: 0x%02X, To: 0x%02X, Group: 0x%02X\n", 
                     header.senderId, header.receiverId, header.groupId);
        Serial.printf("Data: %s\n", data);
    });
    
    // Unhandled events handler
    unhandledDispatcher.on("*", [](const char* data, EventHeader& header) {
        Serial.printf("=== Unhandled Event ===\n");
        Serial.printf("From: 0x%02X, To: 0x%02X, Group: 0x%02X\n", 
                     header.senderId, header.receiverId, header.groupId);
        Serial.printf("Data: %s\n", data);
        Serial.println("Message was not processed by any dispatcher");
    });
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    // Initialize EventMsg with Serial write callback
    eventMsg.init([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
    
    // Set up handlers
    setupHandlers();
    
    // Register dispatchers with EventMsg
    eventMsg.registerDispatcher("loopback", 
                              mainDispatcher.createHeader(0x01), // Direct messages
                              mainDispatcher.getHandler());
                              
    eventMsg.registerDispatcher("broadcast", 
                              mainDispatcher.createHeader(0xFF), // Broadcast messages
                              mainDispatcher.getHandler());
                              
    eventMsg.registerDispatcher("monitor", 
                              monitorDispatcher.createHeader(0xFF), // Monitor all traffic
                              monitorDispatcher.getHandler());
                              
    // Set unhandled event handler
    eventMsg.setUnhandledHandler("unhandled",
                                unhandledDispatcher.createHeader(0xFF),
                                unhandledDispatcher.getHandler());
    
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
            
            // Send as broadcast using dispatcher helper
            auto header = mainDispatcher.createHeader(0xFF); // Broadcast address
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
        
        // Create broadcast header using dispatcher helper
        auto header = mainDispatcher.createHeader(0xFF);
        eventMsg.send("HEARTBEAT", data, header);
        
        lastHeartbeat = millis();
    }
}
