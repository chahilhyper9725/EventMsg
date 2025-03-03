#include <Arduino.h>
#include <EventMsg.h>

EventMsg eventMsg;

// Loopback write callback that processes received data immediately
bool loopbackWrite(uint8_t* data, size_t len) {
    // Process the data we just "sent" by feeding it back into process()
    eventMsg.process(data, len);
    return true;
}

void handleEvent(const char* name, const char* data) {
    // Print received data that was looped back
    Serial.printf("Looped back event '%s' with data: %s\n", name, data);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    // Initialize EventMsg with loopback write callback
    eventMsg.init(loopbackWrite);
    
    // Set device address and group
    eventMsg.setAddr(0x01);
    eventMsg.setGroup(0x00);
    
    // Register event handler
    eventMsg.onEvent(handleEvent);
    
    Serial.println("EventMsg Internal Loopback Test");
    Serial.println("Sending test messages...\n");
    
    // Send a few test messages
    eventMsg.send("TEST1", "Hello World", 0xFF, 0x00, 0x00);
    eventMsg.send("TEST2", "12345", 0xFF, 0x00, 0x00);
    eventMsg.send("TEST3", "!@#$%", 0xFF, 0x00, 0x00);
    
    Serial.println("\nTest complete!");
}

void loop() {
    // Nothing to do here as loopback is handled in the write callback
}
