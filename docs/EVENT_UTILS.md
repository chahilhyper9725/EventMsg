# EventMsgUtils Guide

The EventMsgUtils library provides a flexible and intuitive interface for handling events in the EventMsg system, with special emphasis on BLE and multi-transport integration.

## Basic Usage

### 1. Initialization

```cpp
EventMsg eventMsg;
EventMsgUtils utils(eventMsg);

// Initialize EventMsg with your write callback
eventMsg.init(writeCallback);
```

### 2. Simple Event Handling

```cpp
// Simplest form - just handle data
utils.on("TEMP_UPDATE")
    .handle([](const char* data) {
        Serial.printf("Temperature: %s\n", data);
    });

// With event name and data
utils.on("STATUS")
    .handle([](const char* eventName, const char* data) {
        Serial.printf("Event: %s, Data: %s\n", eventName, data);
    });
```

### 3. Filtered Events

```cpp
// Filter by sender
utils.on("STATUS")
    .from(DEVICE_B)
    .handle([](const char* data) {
        // Only handle STATUS from DEVICE_B
    });

// Filter by group
utils.on("SENSOR_DATA")
    .group(GROUP_SENSORS)
    .handle([](const char* data) {
        // Only handle SENSOR_DATA from sensor group
    });

// Multiple filters
utils.on("PRIORITY_MSG")
    .from(GATEWAY_ID)
    .group(CRITICAL_GROUP)
    .withFlags(FLAG_PRIORITY)
    .handle([](const char* name, const char* data, uint8_t sender) {
        // Complex filtering
    });
```

### 4. Advanced Event Handling

```cpp
// Full header access
utils.on("DEBUG")
    .handle([](const char* name, const char* data,
              const uint8_t* header, uint8_t sender,
              uint8_t receiver, uint8_t flags) {
        // Complete message control
    });

// Raw message access
utils.onRaw()
    .handle([](const uint8_t* data, size_t len) {
        // Handle raw message bytes
    });
```

## BLE Integration

### 1. Basic BLE Setup

```cpp
// Handle incoming BLE messages
utils.on("BLE_MSG")
    .from(BLE_DEVICE)
    .handle([](const char* data) {
        // Process BLE data
    });

// Raw BLE message handling
utils.onRaw()
    .from(BLE_DEVICE)
    .handle([](const uint8_t* data, size_t len) {
        // Direct BLE data access
    });
```

### 2. BLE to ESP-NOW Bridge Pattern

```cpp
class BLEBridge {
    EventMsgUtils& utils;
    
public:
    BLEBridge(EventMsgUtils& u) : utils(u) {
        // Handle BLE messages to forward to ESP-NOW
        utils.on("FORWARD_ESPNOW")
            .from(BLE_DEVICE)
            .handle([this](const char* data, const uint8_t* header) {
                uint8_t targetAddr = header[1];  // Receiver address
                forwardToESPNOW(targetAddr, data);
            });

        // Handle ESP-NOW messages to forward to BLE
        utils.onRaw()
            .from(ESPNOW_DEVICE)
            .handle([this](const uint8_t* data, size_t len) {
                forwardToBLE(data, len);
            });
    }

private:
    void forwardToESPNOW(uint8_t addr, const char* data) {
        // Convert virtual address to ESP-NOW MAC
        uint8_t mac[6];
        if (addressToMAC(addr, mac)) {
            esp_now_send(mac, (uint8_t*)data, strlen(data));
        }
    }

    void forwardToBLE(const uint8_t* data, size_t len) {
        // Forward to connected BLE device
        sendBLENotification(data, len);
    }
};
```

### 3. Device Discovery Pattern

```cpp
// Handle discovery broadcasts
utils.on("DISCOVER")
    .handle([](const char* data, const uint8_t* header, uint8_t sender) {
        // If we match device type
        if (matchDeviceType(data)) {
            // Send response with our info
            char response[32];
            snprintf(response, sizeof(response), 
                    "{type:%d,addr:0x%02X}", DEVICE_TYPE, MY_ADDR);
            eventMsg.send("DISCOVER_RESPONSE", response, sender, 0);
        }
    });

// Handle responses
utils.on("DISCOVER_RESPONSE")
    .handle([](const char* data, uint8_t sender) {
        // Add device to network
        addPeerDevice(data, sender);
    });
```

## Best Practices

1. **Event Naming**
   - Use consistent naming patterns
   - Prefix related events (e.g., SENSOR_TEMP, SENSOR_HUMID)
   - Use clear, descriptive names

2. **Filtering Strategy**
   - Filter as early as possible
   - Combine filters when needed
   - Use group addresses for related devices

3. **Raw Message Handling**
   - Only use raw handlers when necessary
   - Consider message boundaries
   - Handle errors appropriately

4. **Resource Management**
   - Register handlers during setup
   - Avoid creating handlers in loops
   - Clean up resources when done

## Example: Complete Device Setup

```cpp
void setupEventHandlers() {
    // Basic status handling
    utils.on("STATUS")
        .handle([](const char* data) {
            handleStatus(data);
        });

    // Filtered sensor data
    utils.on("SENSOR")
        .group(GROUP_SENSORS)
        .handle([](const char* data, uint8_t sender) {
            processSensorData(data, sender);
        });

    // Raw message handling for bridge
    utils.onRaw()
        .handle([](const uint8_t* data, size_t len) {
            bridgeMessage(data, len);
        });

    // Debug with full info
    utils.on("DEBUG")
        .handle([](const char* name, const char* data,
                  const uint8_t* header, uint8_t sender,
                  uint8_t receiver, uint8_t flags) {
            logDebugInfo(name, data, header);
        });
}
```

This documentation covers the core functionality of EventMsgUtils. The library provides a flexible and intuitive interface for handling various types of events and message patterns in your application.
