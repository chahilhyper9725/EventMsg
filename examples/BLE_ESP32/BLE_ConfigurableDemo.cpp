#include <Arduino.h>
#include <EventMsg.h>
#include <EventDispatcher.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>

// Device addresses
#define DEVICEBROADCAST 0xFF
#define DEVICE01 (uint8_t)0x01
#define DEVICE02 (uint8_t)0x02
#define DEVICE03 (uint8_t)0x03
#define GROUP00 (uint8_t)0x00
#define GROUP01 (uint8_t)0x01

// Nordic UART Service
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// Source configuration
#define BLE_BUFFER_SIZE 1024   // Maximum BLE packet size
#define BLE_QUEUE_SLOTS 16     // Number of queue slots for BLE

EventMsg eventMsg;
uint8_t bleSourceId;           // Will be assigned during setup
EventDispatcher mainDispatcher(DEVICE01);

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pTxCharacteristic = nullptr;
bool deviceConnected = false;
uint32_t bytesReceived = 0;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Device connected");
        auto header = mainDispatcher.createHeader(DEVICEBROADCAST);
        eventMsg.send("ble_connect", "connected", header);
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected");
        pServer->startAdvertising();
        auto header = mainDispatcher.createHeader(DEVICEBROADCAST);
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
                Serial.println("Failed to queue BLE packet - queue full or packet too large");
            }
        }
    }
};

void setup() {
    Serial.begin(115200);
    delay(2000);

    // Create configurable BLE source
    bleSourceId = eventMsg.createSource(BLE_BUFFER_SIZE, BLE_QUEUE_SLOTS);
    Serial.printf("Created BLE source (ID: %d) with buffer: %d, slots: %d\n",
                 bleSourceId, BLE_BUFFER_SIZE, BLE_QUEUE_SLOTS);

    // Initialize NimBLE
    NimBLEDevice::init("EventMsg BLE");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setMTU(512);
    
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(SERVICE_UUID);
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        NIMBLE_PROPERTY::NOTIFY
    );

    NimBLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new CharacteristicCallbacks());

    pService->start();
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    NimBLEDevice::startAdvertising();

    // Set transmit callback
    eventMsg.setWriteCallback([](uint8_t* data, size_t len) {
        if (!deviceConnected) return true;
        
        int mtu = 500;
        int offset = 0;
        for(int i = 0; i < len; i += mtu) {
            int chunksize = std::min(mtu, (int)(len - i));
            pTxCharacteristic->setValue(data + offset, chunksize);
            pTxCharacteristic->notify();
            offset += chunksize;
        }
        return true;
    });

    // Register event handlers
    mainDispatcher.on("command", [](const char* data, EventHeader& header) {
        Serial.printf("Received command: %s\n", data);
        auto response = mainDispatcher.createResponseHeader(header);
        eventMsg.send("command_ack", "success", response);
    });

    // Register dispatcher
    eventMsg.registerDispatcher(
        "main",
        mainDispatcher.createHeader(DEVICE01, GROUP00),
        mainDispatcher.getHandler()
    );

    Serial.println("BLE Server ready with configurable source queue");
}

void loop() {
    // Process messages using new efficient method
    eventMsg.processAllSources();

    // Send status update every 5 seconds if connected
    static unsigned long lastStatus = 0;
    if (deviceConnected && millis() - lastStatus >= 5000) {
        char status[64];
        snprintf(status, sizeof(status),
                 "Uptime: %lus, Bytes: %lu, Source: %d",
                 millis() / 1000, bytesReceived, bleSourceId);

        auto header = mainDispatcher.createHeader(DEVICEBROADCAST);
        eventMsg.send("status", status, header);
        lastStatus = millis();
    }
}
