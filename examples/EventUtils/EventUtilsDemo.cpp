#include <Arduino.h>
#include "EventMsg.h"
#include "EventMsgUtils.h"

// Example custom device addresses and groups
const uint8_t DEVICE_A = 0x01;
const uint8_t DEVICE_B = 0x02;
const uint8_t GROUP_SENSORS = 0x10;
const uint8_t FLAG_PRIORITY = 0x01;

// Global instances
EventMsg eventMsg;
EventMsgUtils* eventUtils;

// Simulated subsystem that needs raw message access
class SubSystem {
public:
    void processRawMessage(const uint8_t* data, size_t len) {
        Serial.print("SubSystem received raw message, length: ");
        Serial.println(len);
        
        // Print raw bytes for demonstration
        Serial.print("Data: ");
        for(size_t i = 0; i < len; i++) {
            Serial.printf("%02X ", data[i]);
        }
        Serial.println();
    }

    void handleParsedStatus(const char* data) {
        Serial.print("SubSystem parsed status: ");
        Serial.println(data);
    }
};

SubSystem subSystem;

// Callback for writing data
bool writeCallback(uint8_t* data, size_t len) {
    // In a real application, this would send the data over a transport layer
    // For demo, we'll loop it back
    eventMsg.process(data, len);
    return true;
}

void setup() {
    Serial.begin(115200);
    Serial.println("EventMsgUtils Demo - Showcasing flexible event handling");
    
    // Initialize EventMsg
    eventMsg.init(writeCallback);
    eventMsg.setAddr(DEVICE_A);
    eventMsg.setGroup(GROUP_SENSORS);
    
    // Create EventMsgUtils
    eventUtils = new EventMsgUtils(eventMsg);
    
    // 1. Simplest form - just handle data
    eventUtils->on("TEMP_UPDATE")
        .handle([](const char* data) {
            Serial.print("Temperature update: ");
            Serial.println(data);
        });

    // 2. With sender filter
    eventUtils->on("STATUS")
        .from(DEVICE_B)
        .handle([](const char* eventName, const char* data) {
            Serial.printf("Status from Device B: Event=%s, Data=%s\n", 
                        eventName, data);
        });

    // 3. With group and flags
    eventUtils->on("SENSOR_DATA")
        .group(GROUP_SENSORS)
        .withFlags(FLAG_PRIORITY)
        .handle([](const char* name, const char* data, uint8_t sender) {
            Serial.printf("Priority sensor data from 0x%02X: %s = %s\n", 
                        sender, name, data);
        });

    // 4. Full control with header access
    eventUtils->on("DEBUG")
        .handle([](const char* name, const char* data, 
                  const uint8_t* header, uint8_t sender,
                  uint8_t receiver, uint8_t flags) {
            Serial.printf("Debug: name=%s, data=%s, sender=0x%02X, receiver=0x%02X, flags=0x%02X\n",
                        name, data, sender, receiver, flags);
        });

    // 5. Raw message handling for subsystem
    eventUtils->onRaw()
        .from(DEVICE_B)
        .handle([](const uint8_t* data, size_t len) {
            subSystem.processRawMessage(data, len);
        });

    // 6. Combining processed and raw handling
    eventUtils->on("STATUS")
        .from(DEVICE_B)
        .handle([](const char* data) {
            subSystem.handleParsedStatus(data);
        });

    Serial.println("Sending test messages...");

    // Send test messages
    eventMsg.send("TEMP_UPDATE", "25.5", DEVICE_B, GROUP_SENSORS);
    eventMsg.send("STATUS", "OK", DEVICE_B, 0);
    eventMsg.send("SENSOR_DATA", "humidity=65%", DEVICE_B, GROUP_SENSORS, FLAG_PRIORITY);
    eventMsg.send("DEBUG", "test message", DEVICE_B, 0);
}

void loop() {
    // In a real application, you would:
    // 1. Read data from transport
    // 2. Call eventMsg.process() with received data
    delay(1000);
}
