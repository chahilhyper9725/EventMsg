#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <esp_now.h>
#include <WiFi.h>
#include "EventMsg.h"
#include "EventMsgUtils.h"

// Constants
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Device types and addresses
const uint8_t DEVICE_TYPE_GATEWAY = 0x01;
const uint8_t DEVICE_TYPE_NODE = 0x02;
const uint8_t BLE_VIRTUAL_ADDR = 0xF0;
const uint8_t MY_ADDR = 0x01;
const uint8_t GROUP_BRIDGE = 0x10;

// Peer entry structure
struct PeerInfo {
    uint8_t mac[6];
    uint8_t virtualAddr;
    bool inUse;
};

// Globals
EventMsg eventMsg;
EventMsgUtils* eventUtils;
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;

// ESP-NOW peer list
PeerInfo peers[20];

class BLEBridgeCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Device connected");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected");
        // Restart advertising
        pServer->startAdvertising();
    }
};

class CharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            // Forward received BLE data to EventMsg system
            eventMsg.process((uint8_t*)value.c_str(), value.length());
        }
    }
};

// Find peer by virtual address
PeerInfo* findPeerByVirtualAddr(uint8_t addr) {
    for (int i = 0; i < 20; i++) {
        if (peers[i].inUse && peers[i].virtualAddr == addr) {
            return &peers[i];
        }
    }
    return nullptr;
}

// ESP-NOW callback when data is received
void OnDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
    // Forward ESP-NOW data to EventMsg system
    eventMsg.process((uint8_t*)data, len);
}

// ESP-NOW callback when data is sent
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2], 
             mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.printf("Last Packet Sent to: %s, status: %s\n", 
                 macStr, status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// EventMsg write callback
bool writeCallback(uint8_t* data, size_t len) {
    if (deviceConnected) {
        pCharacteristic->setValue(data, len);
        pCharacteristic->notify();
    }
    return true;
}

void setup() {
    Serial.begin(115200);
    Serial.println("BLE <-> ESP-NOW Bridge with EventMsgUtils");

    // Initialize WiFi for ESP-NOW
    WiFi.mode(WIFI_STA);
    
    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);
    esp_now_register_send_cb(OnDataSent);

    // Initialize peer list
    memset(peers, 0, sizeof(peers));

    // Initialize BLE
    BLEDevice::init("ESP32 Bridge");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new BLEBridgeCallbacks());

    BLEService* pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->setCallbacks(new CharacteristicCallbacks());
    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    // Initialize EventMsg
    eventMsg.init(writeCallback);
    eventMsg.setAddr(MY_ADDR);
    eventMsg.setGroup(GROUP_BRIDGE);

    // Initialize EventMsgUtils
    eventUtils = new EventMsgUtils(eventMsg);

    // Handle BLE messages to forward to ESP-NOW
    eventUtils->on("FORWARD")
        .from(BLE_VIRTUAL_ADDR)
        .handle([](const char* data, const uint8_t* header) {
            uint8_t targetAddr = header[1];  // Receiver address
            PeerInfo* peer = findPeerByVirtualAddr(targetAddr);
            if (peer) {
                esp_now_send(peer->mac, (uint8_t*)data, strlen(data));
            }
        });

    // Handle discovery messages
    eventUtils->on("DISCOVER")
        .handle([](const char* data) {
            // Send our device info
            char response[64];
            snprintf(response, sizeof(response), 
                    "{\"type\":%d,\"addr\":\"%02X\"}", 
                    DEVICE_TYPE_GATEWAY, MY_ADDR);
            eventMsg.send("DISCOVER_RESPONSE", response, 0xFF, 0, 0);
        });

    // Handle peer registration
    eventUtils->on("REGISTER_PEER")
        .handle([](const char* data) {
            // Find free slot
            int slot = -1;
            for (int i = 0; i < 20; i++) {
                if (!peers[i].inUse) {
                    slot = i;
                    break;
                }
            }
            if (slot == -1) return;

            // Parse MAC and virtual address from data
            // Expected format: "MAC:XX:XX:XX:XX:XX:XX,ADDR:YY"
            uint8_t mac[6];
            uint8_t addr;
            if (sscanf(data, "MAC:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX,ADDR:%02hhX",
                   &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5], &addr) == 7) {
                
                // Add peer
                PeerInfo& peer = peers[slot];
                memcpy(peer.mac, mac, 6);
                peer.virtualAddr = addr;
                peer.inUse = true;

                // Register with ESP-NOW
                esp_now_peer_info_t peerInfo = {};
                memcpy(peerInfo.peer_addr, mac, 6);
                peerInfo.channel = 0;
                peerInfo.encrypt = false;

                if (esp_now_add_peer(&peerInfo) == ESP_OK) {
                    Serial.printf("Added peer with MAC: %02X:%02X:%02X:%02X:%02X:%02X, Virtual Addr: 0x%02X\n",
                                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], addr);

                    // Send confirmation
                    char response[64];
                    snprintf(response, sizeof(response), 
                            "{\"addr\":\"%02X\",\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\"}", 
                            addr, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                    eventMsg.send("PEER_REGISTERED", response, 0xFF, 0, 0);
                }
            }
        });

    Serial.println("Bridge ready!");
}

void loop() {
    delay(10); // Small delay to prevent watchdog triggers
}
