#include <Arduino.h>
#include <EventMsg.h>
#include <EventDispatcher.h>

// Sources with different configurations
uint8_t bleSourceId;      // Large packets, multiple slots
uint8_t uartSourceId;     // Small packets, few slots
uint8_t networkSourceId;  // Medium packets, standard slots
EventMsg eventMsg;

// Dispatcher for demo events
EventDispatcher dispatcher(0x01);

void setup() {
    Serial.begin(115200);
    delay(2000);

    // Create sources with different configurations
    bleSourceId = eventMsg.createSource(1024, 16);     // Large BLE packets
    uartSourceId = eventMsg.createSource(64, 4);       // Small UART packets
    networkSourceId = eventMsg.createSource(512, 8);   // Medium network packets

    // Set write callback separately from init
    eventMsg.setWriteCallback([](uint8_t* data, size_t len) {
        Serial.write(data, len);
        return true;
    });

    // Register event handlers
    dispatcher.on("data", [](const char* data, EventHeader& header) {
        Serial.printf("Received data from source %d\n", header.senderId);
    });

    eventMsg.registerDispatcher(
        "demo",
        dispatcher.createHeader(0x01),
        dispatcher.getHandler()
    );

    Serial.println("Configurable Source Demo Ready");
    Serial.printf("BLE Source ID: %d (1024 bytes, 16 slots)\n", bleSourceId);
    Serial.printf("UART Source ID: %d (64 bytes, 4 slots)\n", uartSourceId);
    Serial.printf("Network Source ID: %d (512 bytes, 8 slots)\n", networkSourceId);
}

// Simulate data from different sources
void simulateBleData() {
    static uint8_t bleData[1000];  // Large BLE packet
    for(int i = 0; i < sizeof(bleData); i++) {
        bleData[i] = i & 0xFF;
    }
    if(sourceManager.pushToSource(bleSourceId, bleData, sizeof(bleData))) {
        Serial.printf("Queued BLE packet (%d bytes)\n", sizeof(bleData));
    } else {
        Serial.println("Failed to queue BLE packet (too large or queue full)");
    }
}

void simulateUartData() {
    static uint8_t uartData[32];  // Small UART packet
    for(int i = 0; i < sizeof(uartData); i++) {
        uartData[i] = i & 0xFF;
    }
    if(sourceManager.pushToSource(uartSourceId, uartData, sizeof(uartData))) {
        Serial.printf("Queued UART packet (%d bytes)\n", sizeof(uartData));
    } else {
        Serial.println("Failed to queue UART packet (too large or queue full)");
    }
}

void simulateNetworkData() {
    static uint8_t networkData[256];  // Medium network packet
    for(int i = 0; i < sizeof(networkData); i++) {
        networkData[i] = i & 0xFF;
    }
    if(sourceManager.pushToSource(networkSourceId, networkData, sizeof(networkData))) {
        Serial.printf("Queued network packet (%d bytes)\n", sizeof(networkData));
    } else {
        Serial.println("Failed to queue network packet (too large or queue full)");
    }
}

void loop() {
    static unsigned long lastSimulation = 0;
    
    // Simulate data reception every second
    if (millis() - lastSimulation >= 1000) {
        simulateBleData();
        simulateUartData();
        simulateNetworkData();
        lastSimulation = millis();
    }

    // Process all sources with one call
    eventMsg.processAllSources();
}
