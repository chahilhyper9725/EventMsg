# Queue-Based Multi-Source Message Handling

This document describes the queue-based multi-source message handling system implemented in the EventMsg library.

## Core Components

### 1. Fixed-Size Packet Structure

```cpp
struct RawPacket {
    static const size_t MAX_SIZE = 512;
    uint8_t sourceId;     // Identify message source
    uint32_t timestamp;   // When packet was received
    uint8_t data[MAX_SIZE];  // Fixed size buffer
    size_t length;
};
```

Features:
- Fixed buffer size eliminates heap fragmentation
- Safe for interrupt context
- Predictable memory usage
- Built-in packet size limits

### 2. Thread-Safe Queue Implementation

```cpp
class ThreadSafeQueue {
    static const size_t QUEUE_SIZE = 8;
    std::array<RawPacket, QUEUE_SIZE> buffer;
    SemaphoreHandle_t mutex;
    size_t head = 0;
    size_t tail = 0;
    bool full = false;
};
```

Features:
- Fixed size circular buffer
- Mutex-protected operations
- Non-blocking tryPop operation
- Overflow protection

### 3. Multi-Source Support

```cpp
// Per-source state tracking
struct ProcessingState {
    ProcessState state;
    std::vector<uint8_t> headerBuffer;
    std::vector<uint8_t> eventNameBuffer;
    std::vector<uint8_t> eventDataBuffer;
    size_t bufferPos;
    bool escapedMode;
};

// Independent state per source
std::array<ProcessingState, 4> sourceStates;
```

Benefits:
- Independent message assembly per source
- No state interference between sources
- Parallel processing capability
- Clear error isolation

## Message Flow

### 1. Reception (e.g., BLE Callback)

```cpp
void onWrite(NimBLECharacteristic *pChar) {
    std::string rxValue = pChar->getValue();
    bool queued = sourceQueues[BLE_SOURCE_ID].push(
        (const uint8_t*)rxValue.data(),
        rxValue.length(),
        BLE_SOURCE_ID
    );
    // Handle queue status
}
```

### 2. Processing Loop

```cpp
void loop() {
    // Process each source independently
    for(int i = 0; i < sourceQueues.size(); i++) {
        RawPacket packet;
        while(sourceQueues[i].tryPop(packet)) {
            eventMsg.process(packet.sourceId, 
                           packet.data, 
                           packet.length);
        }
    }
}
```

## Memory Management

### 1. Static Memory Usage

```cpp
// Per queue overhead
sizeof(RawPacket) * QUEUE_SIZE  // 8 * (512 + 8) bytes per queue
sizeof(ProcessingState) * 4      // State tracking per source
// Total fixed overhead: ~4.2KB per source
```

### 2. Safety Features

- No dynamic allocation in interrupt context
- Protected queue operations
- Buffer overflow prevention
- Queue full detection

## Error Handling

1. **Queue Overflow**
   - Returns false when queue is full
   - Drops packets that exceed MAX_SIZE
   - Logging of dropped packets

2. **Source Isolation**
   - Each source has independent state
   - Errors don't affect other sources
   - Clean state reset per source

## Performance Considerations

### 1. Interrupt Safety

- No memory allocation in callbacks
- Quick queue insertion
- Deferred processing
- Protected shared resources

### 2. Processing Efficiency

- Independent source processing
- Non-blocking queue operations
- Minimal memory copying
- Fixed-size optimizations

## Example Usage

### 1. Adding a New Source

```cpp
#define NEW_SOURCE_ID 2

// In the interrupt/callback:
sourceQueues[NEW_SOURCE_ID].push(
    data_ptr,
    data_length,
    NEW_SOURCE_ID
);
```

### 2. Processing Messages

```cpp
void loop() {
    RawPacket packet;
    // Try to process all queued messages
    while(sourceQueues[NEW_SOURCE_ID].tryPop(packet)) {
        eventMsg.process(packet.sourceId,
                        packet.data,
                        packet.length);
    }
}
```

## Debugging Support

1. **Queue Monitoring**
   - Queue full conditions logged
   - Packet size violations tracked
   - Source-specific error counting

2. **Message Tracing**
   - Per-source message counting
   - Timestamp tracking
   - Queue utilization metrics

## Future Improvements

1. **Potential Enhancements**
   - Priority queuing per source
   - Dynamic queue sizing
   - Zero-copy queue options
   - Source-specific filtering

2. **Optimization Opportunities**
   - DMA support for queue operations
   - Custom memory alignment
   - Source-specific buffer sizes
   - Queue statistics gathering
