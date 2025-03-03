#include <Arduino.h>
#include <EventMsg.h>

// Create EventMsg instance
EventMsg eventMsg;

// LED pin (built-in LED on most ESP32 boards)
const int LED_PIN = 2;

// Function to handle received events
void handleEvent(const char* name, const char* data) {
    if (strcmp(name, "LED_CONTROL") == 0) {
        // Control LED based on received data
        bool state = (data[0] == '1');
        digitalWrite(LED_PIN, state);
        
        // Send acknowledgment
        char response[32];
        snprintf(response, sizeof(response), "LED is now %s", state ? "ON" : "OFF");
        eventMsg.send("LED_STATUS", response, 0xFF, 0x00, 0x00);
    }
    else if (strcmp(name, "PING") == 0) {
        // Respond to ping
        eventMsg.send("PONG", data, 0xFF, 0x00, 0x00);
    }
}

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    // Configure LED pin
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // Initialize EventMsg with Serial write callback
    eventMsg.init([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
    
    // Set device address (1) and group (0)
    eventMsg.setAddr(0x01);
    eventMsg.setGroup(0x00);
    
    // Register event handler
    eventMsg.onEvent(handleEvent);
    
    Serial.println("EventMsg Demo Ready!");
    Serial.println("Commands:");
    Serial.println("1. LED_CONTROL with data '1' or '0'");
    Serial.println("2. PING with any data");
}

void loop() {
    // Process incoming serial data
    while (Serial.available()) {
        uint8_t byte = Serial.read();
        eventMsg.process(&byte, 1);
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
