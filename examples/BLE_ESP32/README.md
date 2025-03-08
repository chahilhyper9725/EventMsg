# BLE Example with Configurable Sources

This example demonstrates how to use BLE with the EventMsg library's configurable source queues. It shows how to:

1. Create a dedicated BLE source with custom buffer size and queue slots
2. Handle BLE data through thread-safe queues
3. Configure write callbacks separately from initialization
4. Process multiple sources efficiently

## Features

- Configurable BLE source:
  - Adjustable buffer size (default: 1024 bytes)
  - Configurable queue slots (default: 16)
  - Independent message processing
  - Thread-safe queue operations

- Improved message handling:
  - No data copying in interrupts
  - Efficient queue-based processing
  - Error handling for queue full/overflow
  - MTU-aware packet handling

## Configuration

You can adjust BLE source settings in BLE_ConfigurableDemo.cpp:

```cpp
#define BLE_BUFFER_SIZE 1024   // Maximum BLE packet size
#define BLE_QUEUE_SLOTS 16     // Number of queue slots
```

Adjust these values based on your requirements:
- Increase buffer size for larger packets
- Add more queue slots for higher throughput
- Decrease values to save memory

## Memory Usage

The BLE source uses:
```
Total Memory = BLE_BUFFER_SIZE * BLE_QUEUE_SLOTS
Example: 1024 * 16 = 16KB for queue storage
```

## Implementation Details

### Creating the BLE Source

```cpp
// Create configurable BLE source
bleSourceId = eventMsg.createSource(BLE_BUFFER_SIZE, BLE_QUEUE_SLOTS);
```

### Handling BLE Data

```cpp
// In BLE callback
bool queued = sourceManager.pushToSource(
    bleSourceId,
    (const uint8_t*)rxValue.data(),
    rxValue.length()
);

// Check queue status
if (!queued) {
    // Handle queue full or packet too large
}
```

### Processing Messages

```cpp
// Process all sources efficiently
void loop() {
    eventMsg.processAllSources();
}
```

## Building

1. Using PlatformIO:
```bash
# For M5Stack Core2
pio run -e m5stack-core2 -t upload
# For ESP32
pio run -e esp32dev -t upload
```

2. Monitor output:
```bash
pio device monitor
```

## Output Example

```
Created BLE source (ID: 1) with buffer: 1024, slots: 16
BLE Server ready with configurable source queue
Device connected
Queued BLE packet of size 128
Processing messages...
Uptime: 10s, Bytes: 128, Source: 1
```

## Error Handling

The example implements comprehensive error handling:

1. Queue Full:
```cpp
if (!sourceManager.pushToSource(...)) {
    Serial.println("Failed to queue - queue full");
}
```

2. Packet Size:
```cpp
if (rxValue.length() > BLE_BUFFER_SIZE) {
    Serial.println("Packet too large");
}
```

3. Connection Status:
```cpp
if (!deviceConnected) {
    return; // Skip transmission when disconnected
}
```

## Debugging

Enable debug logs in platformio.ini:
```ini
build_flags =
    -DENABLE_EVENT_DEBUG_LOGS=1
```

This will show detailed message flow:
```
[EventMsg] Queue status: 2/16 slots used
[EventMsg] Processing packet from source 1 (128 bytes)
[EventMsg] Message complete: command (success)
```

## Memory Optimization

1. Adjust buffer size for your packets:
```cpp
#define BLE_BUFFER_SIZE 512  // Smaller if you don't need 1KB
```

2. Adjust queue slots based on throughput:
```cpp
#define BLE_QUEUE_SLOTS 8   // Fewer slots if processing is fast
```

3. Monitor memory usage:
```cpp
Serial.printf("Free heap: %lu\n", ESP.getFreeHeap());
