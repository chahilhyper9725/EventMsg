#include <Arduino.h>
#include <EventMsg.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>

#define DEVICEBROADCAST 0xFF
#define DEVICE00 0x00
#define DEVICE01 0x01
#define DEVICE02 0x02
#define DEVICE03 0x03
#define DEVICE04 0x04
#define DEVICE05 0x05
#define DEVICE06 0x06

#define GROUP00 0x00
#define GROUP01 0x01
#define GROUP02 0x02
#define GROUP03 0x03
#define GROUP04 0x04
#define GROUP05 0x05
#define GROUP06 0x06

// Nordic UART Service
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

EventMsg msg;
NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pTxCharacteristic = nullptr;
bool deviceConnected = false;
uint32_t msgCount = 0;
uint32_t lastReport = 0;
uint32_t bytesReceived = 0;
uint32_t bytesSent = 0;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *pServer) {
        deviceConnected = true;
        Serial.println("Device connected");
    };

    void onDisconnect(NimBLEServer *pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected");
        // Restart advertising
        pServer->startAdvertising();
    }
};

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            bytesReceived += rxValue.length();
            msg.process((uint8_t *)rxValue.data(), rxValue.length());
        }
    }
};

// Handler for mobile app messages
void mobileAppHandler(const char* deviceName, const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.printf("=== Mobile App Message from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data Length: %d bytes\n", strlen(data));
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X\n", sender, receiver);
    
    // Handle specific events
    if (strcmp(event, "lua_code") == 0) {
        Serial.println("Received Lua code from mobile app");
        // Process Lua code here
    }
    else if (strcmp(event, "config") == 0) {
        Serial.println("Received configuration from mobile app");
        // Apply configuration here
    }
}

// Handler for ESP-NOW messages
void espNowHandler(const char* deviceName, const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.printf("=== ESP-NOW Message from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data Length: %d bytes\n", strlen(data));
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X\n", sender, receiver);
    
    // Handle specific events
    if (strcmp(event, "forward") == 0) {
        Serial.println("Forwarding message to other nodes");
        // Implement message forwarding
    }
    else if (strcmp(event, "status") == 0) {
        Serial.println("Node status update received");
        // Process node status
    }
}

// Raw data handler
void rawDataHandler(const char* deviceName, const char* event, uint8_t* data, size_t length) {
    Serial.printf("=== Raw Data from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Length: %d bytes\n", length);
    Serial.print("Data (hex): ");
    for(size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();
}

// Unhandled event handler with same signature as dispatchers
void unhandledHandler(const char* deviceName, const char* event, const char* data, uint8_t* header, uint8_t sender, uint8_t receiver) {
    Serial.printf("=== Unhandled Event from %s ===\n", deviceName);
    Serial.printf("Event: %s\n", event);
    Serial.printf("Data: %s\n", data);
    Serial.printf("From: 0x%02X, To: 0x%02X\n", sender, receiver);
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    // Initialize NimBLE
    Serial.println("Starting NimBLE Server");
    NimBLEDevice::init("EventMsg BLE");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setMTU(512);
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Create service and characteristics
    NimBLEService *pService = pServer->createService(SERVICE_UUID);
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        NIMBLE_PROPERTY::NOTIFY);

    NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    pRxCharacteristic->setCallbacks(new CharacteristicCallbacks());

    // Start the service
    pService->start();

    // Start advertising
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    NimBLEDevice::startAdvertising();

    Serial.println("BLE Server ready. Waiting for connections...");

    // Initialize EventMsg with BLE transmit callback
    msg.init([](uint8_t *data, size_t len) {
        //write data to BLE characteristic in mtu sized chunks
        int mtu = 500;
        int offset = 0;
        
        for(int i = 0; i < len; i += mtu) {
            int chunksize = mtu < len-i ? mtu : len-i;
            pTxCharacteristic->setValue(data+offset, chunksize);
            pTxCharacteristic->notify();
            offset += chunksize;
        }
        return true;
    });

    // Set local address and group
    msg.setAddr(0x01);
    msg.setGroup(0x00);

    // Register event dispatchers
    msg.registerDispatcher("mobile_app", DEVICE01, GROUP00, mobileAppHandler);  // Mobile app messages
    msg.registerDispatcher("esp_now", DEVICEBROADCAST, GROUP00, espNowHandler); // ESP-NOW broadcasts
    msg.registerDispatcher("esp_now2", DEVICE05, GROUP01, espNowHandler);       // Group 1 messages
    msg.registerDispatcher("esp_now3", DEVICE06, GROUP01, espNowHandler);       // Group 1 messages

    // Register raw handlers for monitoring
    msg.registerRawHandler("raw_monitor", DEVICEBROADCAST, GROUP00, rawDataHandler); // Monitor all messages
    
    // Set handler for unmatched events
    msg.setUnhandledHandler("unhandled", DEVICEBROADCAST, GROUP00, unhandledHandler);

    // Test messages
    msg.send("test_direct", "Hello Device 1", 0x01, 0x00, 0x00);  // Direct message
    msg.send("test_group", "Hello Group 1", 0xFF, 0x01, 0x00);    // Group message
    msg.send("test_broadcast", "Hello All", 0xFF, 0x00, 0x00);    // Broadcast
}

void loop() {
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (c == 't') {  // t[eventname] [eventdata]
            String eventname = Serial.readStringUntil(' ');
            String eventdata = Serial.readStringUntil('\n');
            // Send as broadcast to demonstrate
            msg.send(eventname.c_str(), eventdata.c_str(), 0xFF, 0x00, 0x00);
        }
    }
}
