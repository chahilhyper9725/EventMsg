# Configurable Source Demo

This example demonstrates how to use the new configurable source features of the EventMsg library. It shows how to:

1. Create multiple sources with different configurations
2. Handle different packet sizes per source
3. Use separate write callback configuration
4. Process all sources efficiently

## Features Demonstrated

- Dynamic source creation
- Configurable buffer sizes
- Configurable queue slots
- Source-specific error handling
- Unified source processing

## Source Configurations

The example creates three different sources:

1. BLE Source (Large packets):
   - Buffer size: 1024 bytes
   - Queue slots: 16
   - Suitable for large BLE data transfers

2. UART Source (Small packets):
   - Buffer size: 64 bytes
   - Queue slots: 4
   - Optimized for serial communication

3. Network Source (Medium packets):
   - Buffer size: 512 bytes
   - Queue slots: 8
   - Balanced for typical network traffic

## Usage

1. Configure your source:
```cpp
uint8_t sourceId = eventMsg.createSource(
    bufferSize,  // Maximum packet size
    queueSlots   // Number of queue slots
);
```

2. Push data to source:
```cpp
sourceManager.pushToSource(sourceId, data, length);
```

3. Process all sources in one call:
```cpp
eventMsg.processAllSources();
```

## Memory Usage

Memory is allocated per source based on configuration:
- Each source uses: `bufferSize * queueSlots` bytes
- Example configurations:
  - BLE: 16KB (1024 * 16)
  - UART: 256B (64 * 4)
  - Network: 4KB (512 * 8)

## Error Handling

The example shows how to handle:
- Queue full conditions: Returns false when queue is full
- Packet size violations: Returns false if packet is too large
- Source-specific errors: Each source has independent error handling

## Implementation Details

### Source Creation
```cpp
// Create sources with different configurations
bleSourceId = eventMsg.createSource(1024, 16);     // Large BLE packets
uartSourceId = eventMsg.createSource(64, 4);       // Small UART packets
networkSourceId = eventMsg.createSource(512, 8);   // Medium network packets
```

### Write Callback Setup
```cpp
// Set write callback separately from init
eventMsg.setWriteCallback([](uint8_t* data, size_t len) {
    Serial.write(data, len);
    return true;
});
```

### Data Handling
```cpp
// Push data to source with error handling
if(sourceManager.pushToSource(sourceId, data, length)) {
    // Data queued successfully
} else {
    // Handle queue full or packet too large
}

// Process all sources efficiently
eventMsg.processAllSources();
```

## Building and Running

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

You should see output like:
```
Configurable Source Demo Ready
BLE Source ID: 1 (1024 bytes, 16 slots)
UART Source ID: 2 (64 bytes, 4 slots)
Network Source ID: 3 (512 bytes, 8 slots)
Queued BLE packet (1000 bytes)
Queued UART packet (32 bytes)
Queued network packet (256 bytes)
```

## Troubleshooting

1. Queue Full:
   - Increase queue slots for affected source
   - Process messages more frequently
   - Check for bottlenecks in processing

2. Packet Too Large:
   - Increase buffer size for source
   - Split data into smaller packets
   - Check data size requirements

3. Memory Issues:
   - Reduce buffer sizes or queue slots
   - Monitor heap fragmentation
   - Consider source priority
