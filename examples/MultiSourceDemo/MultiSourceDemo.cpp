#include <Arduino.h>
#include <EventMsg.h>
#include <EventDispatcher.h>

#define SOURCE1_ID 1  // Example: BLE source
#define SOURCE2_ID 2  // Example: UART source
#define SOURCE3_ID 3  // Example: Network source

#define DEVICE01 (uint8_t)0x01
#define GROUP01 (uint8_t)0x01

EventMsg eventMsg;
EventDispatcher dispatcher(DEVICE01);

// Simulated data reception from different sources
void simulateSource1Data() {
    static const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    sourceQueues[SOURCE1_ID].push(data, sizeof(data), SOURCE1_ID);
    Serial.println("Source 1: Queued data");
}

void simulateSource2Data() {
    static const uint8_t data[] = {0x0A, 0x0B, 0x0C, 0x0D};
    sourceQueues[SOURCE2_ID].push(data, sizeof(data), SOURCE2_ID);
    Serial.println("Source 2: Queued data");
}

void simulateSource3Data() {
    static const uint8_t data[] = {0x10, 0x11, 0x12, 0x13};
    sourceQueues[SOURCE3_ID].push(data, sizeof(data), SOURCE3_ID);
    Serial.println("Source 3: Queued data");
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    // Initialize EventMsg
    eventMsg.init([](uint8_t* data, size_t len) {
        Serial.write(data, len);
        return true;
    });

    // Register event handlers
    dispatcher.on("data", [](const char* data, EventHeader& header) {
        Serial.printf("Received data from source %d\n", header.senderId);
    });

    eventMsg.registerDispatcher(
        "demo",
        dispatcher.createHeader(DEVICE01, GROUP01),
        dispatcher.getHandler()
    );

    Serial.println("Multi-Source Demo Ready");
}

void loop() {
    static unsigned long lastSimulation = 0;
    
    // Simulate data reception every second
    if (millis() - lastSimulation >= 1000) {
        simulateSource1Data();
        simulateSource2Data();
        simulateSource3Data();
        lastSimulation = millis();
    }

    // Process messages from all sources
    for(int i = 0; i < sourceQueues.size(); i++) {
        RawPacket packet;
        while(sourceQueues[i].tryPop(packet)) {
            Serial.printf("Processing packet from source %d (size: %d)\n", 
                         packet.sourceId, packet.length);
            
            eventMsg.process(packet.sourceId, packet.data, packet.length);
        }
    }
}
