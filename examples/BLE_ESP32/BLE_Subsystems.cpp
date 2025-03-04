#include <Arduino.h>
#include <EventMsg.h>
#include <EventDispatcher.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>

#define DEVICEBROADCAST 0xFF
#define DEVICE01 0x01
#define GROUP00 0x00

// Nordic UART Service UUIDs
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

EventMsg eventMsg;

// Each subsystem gets its own dispatcher
struct FileSubsystem {
    EventDispatcher dispatcher;
    
    FileSubsystem() : dispatcher(DEVICE01) {
        // File operation handlers
        dispatcher.on("write", [this](const char* data, const EventHeader& header) {
            Serial.printf("Writing file: %s\n", data);
            auto response = dispatcher.createResponseHeader(header);
            eventMsg.send("writeComplete", "success", response);
        });
        
        dispatcher.on("read", [this](const char* data, const EventHeader& header) {
            Serial.printf("Reading file: %s\n", data);
            auto response = dispatcher.createResponseHeader(header);
            eventMsg.send("readData", "file_content", response);
        });
    }
    
    void registerWithEventMsg() {
        eventMsg.registerDispatcher("file", 
            dispatcher.createHeader(DEVICE01),
            dispatcher.getHandler());
    }
};

struct SensorSubsystem {
    EventDispatcherInfo dispatcher;
    float lastTemp = 25.0f;
    float lastHumidity = 60.0f;
    
    SensorSubsystem() : dispatcher(DEVICE01) {
        dispatcher.on("readTemp", [this](const char* data, const EventHeader& header) {
            char response[32];
            snprintf(response, sizeof(response), "%.1f", lastTemp);
            auto responseHeader = dispatcher.createResponseHeader(header);
            eventMsg.send("tempData", response, responseHeader);
        });
        
        dispatcher.on("readHumidity", [this](const char* data, const EventHeader& header) {
            char response[32];
            snprintf(response, sizeof(response), "%.1f", lastHumidity);
            auto responseHeader = dispatcher.createResponseHeader(header);
            eventMsg.send("humidityData", response, responseHeader);
        });
    }
    
    void registerWithEventMsg() {
        eventMsg.registerDispatcher("sensor", 
            dispatcher.createHeader(DEVICE01),
            dispatcher.getHandler());
    }
    
    void updateReadings() {
        // Simulate sensor readings
        lastTemp += random(-10, 10) / 10.0f;
        lastHumidity += random(-5, 5) / 10.0f;
        
        if (lastTemp < 20.0f) lastTemp = 20.0f;
        if (lastTemp > 30.0f) lastTemp = 30.0f;
        if (lastHumidity < 40.0f) lastHumidity = 40.0f;
        if (lastHumidity > 80.0f) lastHumidity = 80.0f;
    }
};

// Create subsystems
FileSubsystem fileSystem;
SensorSubsystem sensorSystem;

// BLE Server components
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pTxCharacteristic = nullptr;
bool deviceConnected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Client connected");
    }

    void onDisconnect(NimBLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Client disconnected");
        pServer->startAdvertising();
    }
};

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            eventMsg.process((uint8_t*)rxValue.data(), rxValue.length());
        }
    }
};

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    // Initialize BLE
    NimBLEDevice::init("EventMsg BLE");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    
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
    
    // Initialize EventMsg with BLE callback
    eventMsg.init([](uint8_t* data, size_t len) {
        if (!deviceConnected) return true;
        int offset = 0;
        int mtu = 500;
        for(int i = 0; i < len; i += mtu) {
            int chunksize = mtu < len-i ? mtu : len-i;
            pTxCharacteristic->setValue(data+offset, chunksize);
            pTxCharacteristic->notify();
            offset += chunksize;
        }
        return true;
    });
    
    // Register subsystems
    fileSystem.registerWithEventMsg();
    sensorSystem.registerWithEventMsg();
    
    Serial.println("BLE Subsystems Demo Ready");
    Serial.println("Commands:");
    Serial.println("1. write <filename> - Simulate file write");
    Serial.println("2. read <filename> - Simulate file read");
    Serial.println("3. temp - Read temperature");
    Serial.println("4. humidity - Read humidity");
}

void loop() {
    // Handle serial commands for testing
    if (Serial.available()) {
        String cmd = Serial.readStringUntil(' ');
        String data = Serial.readStringUntil('\n');
        
        if (cmd == "write" || cmd == "read") {
            auto header = fileSystem.dispatcher.createHeader(DEVICE01);
            eventMsg.send(cmd.c_str(), data.c_str(), header);
        }
        else if (cmd == "temp") {
            auto header = sensorSystem.dispatcher.createHeader(DEVICE01);
            eventMsg.send("readTemp", "request", header);
        }
        else if (cmd == "humidity") {
            auto header = sensorSystem.dispatcher.createHeader(DEVICE01);
            eventMsg.send("readHumidity", "request", header);
        }
    }
    
    // Update sensor readings periodically
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 1000) {
        sensorSystem.updateReadings();
        lastUpdate = millis();
    }
}
