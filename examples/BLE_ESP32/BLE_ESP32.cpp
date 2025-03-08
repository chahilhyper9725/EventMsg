#include <Arduino.h>
#include <EventMsg.h>
#include <EventDispatcher.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>

#define DEVICEBROADCAST 0xFF
#define DEVICE01 (uint8_t)0x01
#define DEVICE02 (uint8_t)0x02
#define DEVICE03 (uint8_t)0x03
#define GROUP00 (uint8_t)0x00
#define GROUP01 (uint8_t)0x01

// Nordic UART Service UUIDs
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

EventMsg eventMsg;
uint8_t bleSourceId;  // Will be assigned during setup

// Create separate dispatchers for different message types
EventDispatcher mobileDispatcher(DEVICE01); // Mobile app messages
EventDispatcher espNowDispatcher(DEVICE01); // ESP-NOW messages
EventDispatcher bleDispatcher(DEVICE01);    // BLE control messages

// BLE globals
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pTxCharacteristic = nullptr;
bool deviceConnected = false;
uint32_t bytesReceived = 0;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Device connected");
        auto header = bleDispatcher.createHeader(DEVICEBROADCAST);
        eventMsg.send("ble_connect", "connected", header);
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected");
        pServer->startAdvertising();
        auto header = bleDispatcher.createHeader(DEVICEBROADCAST);
        eventMsg.send("ble_disconnect", "disconnected", header);
    }
};

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            bytesReceived += rxValue.length();
            
            // Queue received data with source-specific configuration
            bool queued = sourceManager.pushToSource(
                bleSourceId,
                (const uint8_t*)rxValue.data(),
                rxValue.length()
            );
            
            if (queued) {
                Serial.printf("Queued BLE packet of size %d\n", rxValue.length());
            } else {
                Serial.println("Failed to queue BLE packet - queue full or too large");
            }
        }
    }
};

// Example event handler
void lua_code(const char* data, EventHeader& header) {
    Serial.println("Received Lua code from mobile app");
    Serial.println(data);
    auto responseHeader = mobileDispatcher.createResponseHeader(header);
    eventMsg.send("lua_result", "success", responseHeader);
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    // Create BLE source with large buffer for BLE packets
    bleSourceId = eventMsg.createSource(1024, 16);
    Serial.printf("Created BLE source (ID: %d) with 1KB buffer, 16 slots\n", bleSourceId);

    // Initialize NimBLE
    Serial.println("Starting NimBLE Server");
    NimBLEDevice::init("EventMsg BLE");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setMTU(512);
    
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Create service and characteristics
    NimBLEService* pService = pServer->createService(SERVICE_UUID);
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        NIMBLE_PROPERTY::NOTIFY);

    NimBLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    pRxCharacteristic->setCallbacks(new CharacteristicCallbacks());

    pService->start();
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    NimBLEDevice::startAdvertising();

    // Set up write callback
    eventMsg.setWriteCallback([](uint8_t* data, size_t len) {
        if (!deviceConnected) return true;
        
        int mtu = 500;
        int offset = 0;
        for(int i = 0; i < len; i += mtu) {
          int chunksize = mtu < len-i ? mtu : len-i;
            pTxCharacteristic->setValue(data + offset, chunksize);
            pTxCharacteristic->notify();
            offset += chunksize;
        }
        return true;
    });

    // Set up mobile app event handlers
    mobileDispatcher.on("lua_code", lua_code);
    mobileDispatcher.on("config", [](const char* data, EventHeader& header) {
        Serial.println("Received configuration from mobile app");
        auto responseHeader = mobileDispatcher.createResponseHeader(header);
        eventMsg.send("config_applied", "success", responseHeader);
    });

    // Set up ESP-NOW event handlers
    espNowDispatcher.on("forward", [](const char* data, EventHeader& header) {
        Serial.println("Forwarding message to other nodes");
        auto broadcastHeader = espNowDispatcher.createHeader(DEVICEBROADCAST);
        eventMsg.send("forward_msg", data, broadcastHeader);
    });

    // Set up BLE control event handlers
    bleDispatcher.on("mtu_update", [](const char* data, EventHeader& header) {
        int mtu = atoi(data);
        NimBLEDevice::setMTU(mtu);
        auto responseHeader = bleDispatcher.createResponseHeader(header);
        eventMsg.send("mtu_updated", data, responseHeader);
    });

    // Register dispatchers with EventMsg
    eventMsg.registerDispatcher("mobile_app",
                              mobileDispatcher.createHeader(DEVICE01, GROUP01),
                              mobileDispatcher.getHandler());
                              
    eventMsg.registerDispatcher("esp_now",
                              espNowDispatcher.createHeader(DEVICE02, GROUP01),
                              espNowDispatcher.getHandler());
                              
    eventMsg.registerDispatcher("ble_control",
                              bleDispatcher.createHeader(DEVICE03, GROUP00),
                              bleDispatcher.getHandler());

    Serial.println("BLE Server ready with configurable queue");
}

void loop() {
    // Process all sources efficiently
    eventMsg.processAllSources();

    // Process incoming serial data (for testing)
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 't') {  // Test commands
            String command = Serial.readStringUntil(' ');
            String data = Serial.readStringUntil('\n');

            // Choose appropriate dispatcher based on command
            if (command == "lua" || command == "config") {
                auto header = mobileDispatcher.createHeader(DEVICE01);
                eventMsg.send(command.c_str(), data.c_str(), header);
            }
            else if (command == "forward") {
                auto header = espNowDispatcher.createHeader(DEVICEBROADCAST);
                eventMsg.send(command.c_str(), data.c_str(), header);
            }
            else if (command == "mtu") {
                auto header = bleDispatcher.createHeader(DEVICE01);
                eventMsg.send("mtu_update", data.c_str(), header);
            }
        }
    }

    // Send status every 5 seconds if connected
    static unsigned long lastStatus = 0;
    if (deviceConnected && millis() - lastStatus >= 5000) {
        char status[64];
        snprintf(status, sizeof(status), "Uptime: %lus, Bytes received: %lu",
                 millis() / 1000, bytesReceived);

        auto header = bleDispatcher.createHeader(DEVICEBROADCAST);
        eventMsg.send("ble_status", status, header);
        lastStatus = millis();
    }
}
