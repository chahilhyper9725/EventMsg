#include <Arduino.h>
#include <EventMsg.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>

// Nordic UART Service
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// Queue size should be power of 2 for efficient wrapping
#define QUEUE_SIZE 16
#define QUEUE_MASK (QUEUE_SIZE - 1)
#define MAX_MSG_SIZE 512

EventMsg msg;
NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pTxCharacteristic = nullptr;
bool deviceConnected = false;
uint32_t msgCount = 0;
uint32_t lastReport = 0;
uint32_t bytesReceived = 0;
uint32_t bytesSent = 0;

// Ring buffer for incoming messages
struct MessageBuffer
{
    uint8_t data[MAX_MSG_SIZE];
    size_t length;
};

struct MessageQueue
{
    MessageBuffer buffers[QUEUE_SIZE];
    volatile uint16_t writeIndex;
    volatile uint16_t readIndex;
} queue;

// Add message to queue
bool queueMessage(const uint8_t *data, size_t length)
{
    if (length > MAX_MSG_SIZE)
    {
        DEBUG_PRINT("Message too large: %d bytes", length);
        return false;
    }

    uint16_t nextWrite = (queue.writeIndex + 1) & QUEUE_MASK;
    if (nextWrite == queue.readIndex)
    {
        DEBUG_PRINT("Queue full");
        return false;
    }

    MessageBuffer &buffer = queue.buffers[queue.writeIndex];
    memcpy(buffer.data, data, length);
    buffer.length = length;

    queue.writeIndex = nextWrite;
    return true;
}

// Process next message in queue
void processQueue()
{
    if (queue.readIndex == queue.writeIndex)
    {
        return; // Queue empty
    }

    MessageBuffer &buffer = queue.buffers[queue.readIndex];
    msg.process(buffer.data, buffer.length);

    queue.readIndex = (queue.readIndex + 1) & QUEUE_MASK;
}

class ServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *pServer)
    {
        deviceConnected = true;
        Serial.println("Device connected");
    };

    void onDisconnect(NimBLEServer *pServer)
    {
        deviceConnected = false;
        Serial.println("Device disconnected");
        // Restart advertising
        pServer->startAdvertising();
    }
};

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *pCharacteristic)
    {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0)
        {
            bytesReceived += rxValue.length();

            // Serial.printf("=== Received BLE Data ===\n");
            // Serial.printf("Length: %d bytes\n", rxValue.length());
            // Serial.print("Raw Hex: ");
            // for (size_t i = 0; i < rxValue.length(); i++) {
            //     Serial.printf("%02X ", rxValue[i]);
            //     if ((i + 1) % 16 == 0) Serial.println();
            // }
            // Serial.println();

            if (queueMessage((uint8_t *)rxValue.data(), rxValue.length()))
            {
                Serial.println("Message queued successfully");
            }
            else
            {
                Serial.println("Failed to queue message");
            }
            Serial.println("=====================\n");
        }
    }
};

void setup()
{
    Serial.begin(115200);
    delay(2000);

    // Initialize NimBLE
    Serial.println("Starting NimBLE Server");
    NimBLEDevice::init("EventMsg BLE");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Create server
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
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR); // Add No Response option
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
    msg.init([](uint8_t *data, size_t len)
             {
        if (deviceConnected && pTxCharacteristic != nullptr) {
            pTxCharacteristic->setValue(data, len);
            pTxCharacteristic->notify();
            bytesSent += len;
            msgCount++;
            return true;
        }
        return false; });

    // Set local address and group
    msg.setAddr(0x01);
    msg.setGroup(0x00);

    // Register event handler for received messages
    msg.onEvent([](const char *event, const char *data)
                {
                    Serial.printf("=== Message Received ===\n");
                    Serial.printf("Event: %s\n", event);
                    Serial.printf("Data: %s\n", data);
                    Serial.printf("Data Length: %d bytes\n", strlen(data));
                    Serial.println("====================="); });
}

void loop()
{
    // Process any queued messages
    processQueue();

    // Print throughput stats every second
    // if (millis() - lastReport >= 1000)
    // {
    //     if (deviceConnected)
    //     {
    //         float kbReceived = bytesReceived / 1024.0f;
    //         float kbSent = bytesSent / 1024.0f;
    //         Serial.printf("Throughput - RX: %.2f KB/s, TX: %.2f KB/s, Messages: %u\n",
    //                       kbReceived, kbSent, msgCount);
    //     }
    //     bytesReceived = 0;
    //     bytesSent = 0;
    //     msgCount = 0;
    //     lastReport = millis();
    // }

    if (Serial.available() > 0)
    {
        char c = Serial.read();
        if (c == 's')
        {
            msg.send("test", "test", 0x01, 0x00, 0x00);
        }
        if (c == 't')//t[eventname] [eventdata] //no [] in actual command tTestEvent TestEventData
        {
            String eventname = Serial.readStringUntil(' ');
            String eventdata = Serial.readStringUntil('\n');
            msg.send(eventname.c_str(), eventdata.c_str(), 0x01, 0x00, 0x00);
        }

    }
    // delay(5); // Small delay to prevent watchdog issues
}

