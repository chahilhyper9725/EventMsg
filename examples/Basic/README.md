# Basic EventMsg Example

This example demonstrates the core functionality of the EventMsg library using Serial communication on an ESP32.

## Features

- Modern EventDispatcher usage
- LED control via events with header management
- Heartbeat messages with broadcast headers
- Ping/Pong functionality with automatic response headers
- Event monitoring and unhandled event capture

## Hardware Required

- ESP32 development board
- USB cable
- Built-in LED (or external LED connected to GPIO 2)

## Installation

1. Open this example in PlatformIO
2. Connect your ESP32 board
3. Build and upload the code

## Code Overview

The example uses the modern EventDispatcher pattern:

```cpp
// Create dispatchers for different purposes
EventDispatcher mainDispatcher(0x01);    // Local address 0x01
EventDispatcher monitorDispatcher(0x01); // For monitoring traffic

// Setup event handlers
mainDispatcher.on("LED_CONTROL", [](const char* data, EventHeader& header) {
    bool state = (data[0] == '1');
    digitalWrite(LED_PIN, state);
    
    // Create response header automatically
    auto responseHeader = mainDispatcher.createResponseHeader(header);
    eventMsg.send("LED_STATUS", state ? "ON" : "OFF", responseHeader);
});

// Monitor all traffic
monitorDispatcher.on("*", [](const char* data, EventHeader& header) {
    Serial.printf("=== Monitor Traffic ===\n");
    Serial.printf("From: 0x%02X, To: 0x%02X, Group: 0x%02X\n", 
                 header.senderId, header.receiverId, header.groupId);
    Serial.printf("Data: %s\n", data);
});

// Register dispatchers
eventMsg.registerDispatcher("main", 
                          mainDispatcher.createHeader(0x01), 
                          mainDispatcher.getHandler());
```

## Usage

After uploading the code, open a serial monitor at 115200 baud. You can use PlatformIO's built-in serial monitor:

```bash
pio device monitor
```

### Available Commands

The example accepts the following events over serial:

1. LED Control:
   ```
   LED_CONTROL with data "1" (ON) or "0" (OFF)
   ```
   - Response: `LED_STATUS` event with current LED state
   - Automatically uses response header for reply

2. Ping Test:
   ```
   PING with any data
   ```
   - Response: `PONG` event with the same data echoed back
   - Uses createResponseHeader for proper routing

### Automatic Messages

The device automatically sends:
- `HEARTBEAT` event every 5 seconds using broadcast header
- Each dispatcher sends status updates with proper headers

### Message Headers

The example demonstrates different header types:

```cpp
// Direct message header
auto directHeader = mainDispatcher.createHeader(0x01);

// Broadcast header
auto broadcastHeader = mainDispatcher.createHeader(0xFF);

// Response header (automatically sets sender/receiver)
auto responseHeader = mainDispatcher.createResponseHeader(receivedHeader);
```

## Protocol Details

Messages use the EventMsg protocol format with EventHeader:
```
[SOH][Header][STX][Event Name][US][Event Data][EOT]

Header = {
    senderId,    // Source device
    receiverId,  // Destination (0xFF for broadcast)
    groupId,     // Message group
    flags        // Control flags
}
```

For more details about the protocol, see the [main documentation](../../docs/PROTOCOL.md).

## Troubleshooting

1. If you don't see any output:
   - Check if you're using the correct serial port
   - Verify the baud rate is set to 115200
   - Reset the ESP32 board

2. If LED doesn't respond:
   - Verify the LED pin number (default GPIO 2)
   - Check LED polarity if using external LED
   - Verify message headers are correctly set
   - Monitor traffic using the monitor dispatcher

## Next Steps

1. Try modifying the example to:
   - Add new event types with their own dispatchers
   - Use group messaging features
   - Add more sensors with dedicated dispatchers
   - Implement bidirectional communication

2. Explore other examples:
   - [ESP32 BLE Example](../BLE_ESP32) for wireless communication
   - [Multiple Dispatcher Demo](../EventDispatcherDemo) for subsystem organization
   - [Basic Loopback](../BasicLoopback) for protocol testing
