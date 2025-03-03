# EventMsg Library

A lightweight event-based messaging protocol library for ESP32 and Arduino platforms. This library provides a reliable communication protocol with features like byte stuffing, message framing, and addressing support.

## Features

- 🔄 Reliable message framing with byte stuffing
- 📫 Addressing system with broadcast support
- 👥 Group-based message filtering
- 🔌 Transport layer agnostic (UART, TCP, BLE, etc.)
- 🛡️ Buffer overflow protection
- 🧩 Event-based architecture
- 📚 Comprehensive documentation

## Installation

### PlatformIO

1. Add dependency to your `platformio.ini`:

```ini
lib_deps = 
    EventMsg
```

### Manual Installation

1. Download this repository
2. Copy it to your Arduino/libraries folder
3. Restart your Arduino IDE

## Quick Start

### Basic Usage

```cpp
#include <EventMsg.h>

EventMsg eventMsg;

void setup() {
    Serial.begin(115200);
  
    // Initialize with Serial write callback
    eventMsg.init([](uint8_t* data, size_t len) {
        return Serial.write(data, len) == len;
    });
  
    // Set device address and group
    eventMsg.setAddr(0x01);
    eventMsg.setGroup(0x00);
  
    // Register event handler
    eventMsg.onEvent([](const char* name, const char* data) {
        if (strcmp(name, "LED_CONTROL") == 0) {
            // Handle LED control event
            digitalWrite(LED_BUILTIN, data[0] == '1');
        }
    });
}

void loop() {
    // Check for incoming data
    while (Serial.available()) {
        uint8_t byte = Serial.read();
        eventMsg.process(&byte, 1);
    }
  
    // Send temperature reading every second
    static unsigned long lastSend = 0;
    if (millis() - lastSend >= 1000) {
        float temp = readTemperature(); // Your temperature reading function
        char data[32];
        snprintf(data, sizeof(data), "%.1f", temp);
        eventMsg.send("TEMP_UPDATE", data, 0xFF, 0x00, 0x00);
        lastSend = millis();
    }
}
```

### ESP32 BLE Example

See [examples/BLE_ESP32](examples/BLE_ESP32) for a complete BLE communication example.

## Protocol Details

The EventMsg protocol uses a simple message format with proper framing:

```
[SOH][Header][STX][Event Name][US][Event Data][EOT]
```

For detailed protocol documentation including message format, control characters, byte stuffing algorithm, and state machine implementation, see [docs/PROTOCOL.md](docs/PROTOCOL.md).

For detailed implementation documentation including core components, memory management, thread safety, and performance optimizations, see [docs/IMPLEMENTATION.md](docs/IMPLEMENTATION.md).

## Development Tools

The library comes with web-based tools to help with development and debugging:

- **Protocol Debugger** ([docs/webtools/protocol-debugger.html](docs/webtools/protocol-debugger.html)) - Interactively explore the protocol structure and decode messages
- **Protocol Editor** ([docs/webtools/protocol-editor.html](docs/webtools/protocol-editor.html)) - Visual tool to create and edit protocol messages
- **BLE Tester** ([docs/webtools/ble-tester.html](docs/webtools/ble-tester.html)) - Test BLE communication using the EventMsg protocol

## API Reference

### Class: EventMsg

#### Methods

##### `bool init(WriteCallback cb)`

Initialize the EventMsg instance with a write callback

- `cb`: Function to handle data transmission
- Returns: `true` if initialization successful

##### `void setAddr(uint8_t addr)`

Set the local device address

- `addr`: Device address (0x00-0xFF)

##### `void setGroup(uint8_t addr)`

Set the device group address

- `addr`: Group address (0x00-0xFF)

##### `size_t send(const char* name, const char* data, uint8_t recvAddr, uint8_t groupAddr, uint8_t flags)`

Send an event message

- `name`: Event name string
- `data`: Event data string
- `recvAddr`: Receiver address (0xFF for broadcast)
- `groupAddr`: Group address (0x00 for no group)
- `flags`: Message flags
- Returns: Number of bytes sent, or 0 on failure

##### `bool process(uint8_t* data, size_t len)`

Process received data

- `data`: Pointer to received data
- `len`: Length of received data
- Returns: `true` if valid message was processed

##### `void onEvent(EventCallback cb, uint8_t receiverId = 0xFF, uint8_t groupId = 0)`

Register event callback

- `cb`: Callback function `void(const char* name, const char* data)`
- `receiverId`: Filter by receiver ID (optional)
- `groupId`: Filter by group ID (optional)

## Limitations

- Maximum event name size: 32 bytes
- Maximum event data size: 2048 bytes
- Maximum header size: 6 bytes

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Inspired by various messaging protocols and event systems
- Thanks to all contributors and users
