# EventMsgV3 TypeScript

> A modern, type-safe, transport-agnostic messaging library for ESP32 and web applications

[![TypeScript](https://img.shields.io/badge/TypeScript-007ACC?style=flat&logo=typescript&logoColor=white)](https://www.typescriptlang.org/)
[![WebBLE](https://img.shields.io/badge/WebBLE-Ready-blue?style=flat)](https://developer.mozilla.org/en-US/docs/Web/API/Web_Bluetooth_API)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

EventMsgV3-TS provides a robust, binary protocol for reliable messaging between web browsers and ESP32 devices. Built with TypeScript-first design, it offers complete type safety, multiple transport options, and a clean async/await API.

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Core Concepts](#core-concepts)
- [API Reference](#api-reference)
- [Available Transports](#available-transports)
- [TypeScript Features](#typescript-features)
- [Advanced Usage](#advanced-usage)
- [Examples](#examples)
- [Protocol Details](#protocol-details)
- [Contributing](#contributing)

## Features

✨ **Modern TypeScript** - Full type safety with generics and strict typing  
🔌 **Transport Agnostic** - WebBLE, Serial, TCP/WebSocket support  
📡 **Binary Protocol** - Efficient EventMsgV3 protocol with byte stuffing  
🎯 **Device Addressing** - Unicast, broadcast, and group messaging  
⚡ **Async/Await** - Promise-based API with timeout support  
🔄 **Request/Response** - Built-in `waitFor` pattern for RPC-style calls  
🛡️ **Error Handling** - Comprehensive error types with context  
🔍 **Debug Support** - Detailed logging and message inspection  
🌐 **Browser Ready** - Web Bluetooth for ESP32 communication  

## Installation

> 🚧 **Coming Soon** - Package will be available on npm

```bash
# Core library
npm install @eventmsg/core

# WebBLE transport for browsers
npm install @eventmsg/transport-webble

# Additional transports (planned)
npm install @eventmsg/transport-serial    # Node.js serial
npm install @eventmsg/transport-websocket # Network transport
```

### Browser CDN Usage

Use directly in browser with ES modules via CDN:

```html
<script type="importmap">
{
  "imports": {
    "@eventmsg/core": "https://cdn.jsdelivr.net/npm/@eventmsg/core@latest",
    "@eventmsg/transport-webble": "https://cdn.jsdelivr.net/npm/@eventmsg/transport-webble@latest"
  }
}
</script>
<script type="module">
  import { EventMsg } from '@eventmsg/core';
  import { WebBLETransport, createNordicUARTConfig } from '@eventmsg/transport-webble';
  
  // Same API as npm usage
  const transport = new WebBLETransport(createNordicUARTConfig(0, 0));
  const eventMsg = new EventMsg({ transport });
</script>
```
 
## Quick Start

```typescript
import { EventMsg } from '@eventmsg/core';
import { WebBLETransport, createNordicUARTConfig } from '@eventmsg/transport-webble';

// Create transport with device addressing
const config = createNordicUARTConfig(0, 0); // localAddress=0, groupAddress=0
const transport = new WebBLETransport(config);

// Create EventMsg instance
const eventMsg = new EventMsg({ transport, debug: true });

// Connect to ESP32
await eventMsg.connect();

// Send a message
await eventMsg.send('sensor_request', { type: 'temperature' }, {
  receiverId: 1 // Send to device 1
});

// Listen for responses
eventMsg.onMessage('sensor_data', (data, metadata) => {
  console.log(`Temperature: ${data.value}°C from device ${metadata.senderId}`);
});

// Request/response pattern
const response = await eventMsg.waitFor('ping_response', { timeout: 5000 });
console.log('Ping RTT:', Date.now() - response.data.timestamp);
```

## Core Concepts

### Clean API Separation

EventMsgV3 separates lifecycle events from user messages to prevent namespace conflicts:

```typescript
// Lifecycle events (use .on())
eventMsg.on('connect', () => console.log('Connected!'));
eventMsg.on('disconnect', () => console.log('Disconnected'));
eventMsg.on('error', (error) => console.error('Error:', error));

// User messages (use .onMessage())
eventMsg.onMessage((eventName, data, metadata) => {
  // Handle any incoming message
});

eventMsg.onMessage('ping', (data, metadata) => {
  // Handle specific message types
});
```

### Device Addressing

EventMsgV3 supports flexible addressing modes:

| Mode | Description | Example |
|------|-------------|---------|
| **Unicast** | Send to specific device | `receiverId: 5` |
| **Broadcast** | Send to all devices | `receiverId: 255` |
| **Group** | Send to device group | `receiverGroupId: 1` |
| **Group Broadcast** | Send to all in group | `receiverId: 255, receiverGroupId: 1` |

### Message Structure

Every message includes rich metadata for routing and debugging:

```typescript
interface MessageMetadata {
  senderId: number;         // Who sent it (0-255)
  senderGroupId: number;    // Sender's group (0-255)  
  receiverId: number;       // Target device (0-255)
  receiverGroupId: number;  // Target group (0-255)
  messageId: number;        // Sequence number (0-65535)
  flags: number;           // Custom flags (0-255)
  timestamp: Date;         // When received
}
```

## API Reference

### EventMsg Class

#### Connection Management

```typescript
// Connect to transport
await eventMsg.connect(): Promise<void>

// Disconnect gracefully  
await eventMsg.disconnect(): Promise<void>

// Check connection status
eventMsg.isConnected(): boolean

// Get device info
eventMsg.getLocalAddress(): number
eventMsg.getGroupAddress(): number
eventMsg.getStats(): ConnectionStats
```

#### Sending Messages

```typescript
// Basic send with addressing
await eventMsg.send<TData>(
  event: string,
  data: TData, 
  options: SendOptions
): Promise<void>

interface SendOptions {
  receiverId: number;           // Target device (required)
  receiverGroupId?: number;     // Target group (optional)
  flags?: number;              // Custom flags (optional)  
  timeout?: number;            // Send timeout (optional)
}
```

#### Receiving Messages

```typescript
// Listen for all messages
eventMsg.onMessage((eventName, data, metadata) => {
  console.log(`Received ${eventName} from device ${metadata.senderId}`);
});

// Listen for specific events with type safety
eventMsg.onMessage<{temperature: number}>('sensor_data', (data, metadata) => {
  console.log(`Temperature: ${data.temperature}°C`);
});

// Remove listeners
eventMsg.offMessage('sensor_data'); // Remove specific
eventMsg.offMessage();              // Remove all
```

#### Request/Response Pattern

```typescript
// Wait for specific response
const result = await eventMsg.waitFor<TData>(
  event: string,
  options?: WaitForOptions
): Promise<MessageResult<TData>>

interface WaitForOptions {
  timeout?: number;                              // Default: 5000ms
  filter?: (metadata: MessageMetadata) => boolean; // Message filter
}

// Example: Ping with filtering
const pong = await eventMsg.waitFor('pong', {
  timeout: 3000,
  filter: (meta) => meta.senderId === targetDevice
});
```

## Available Transports

### WebBLE Transport

Perfect for browser-to-ESP32 communication using Web Bluetooth:

```typescript
import { WebBLETransport, createNordicUARTConfig } from '@eventmsg/transport-webble';

// Quick setup with Nordic UART Service
const config = createNordicUARTConfig(localAddr, groupAddr);
const transport = new WebBLETransport(config);

// Custom configuration
const customConfig: WebBLETransportConfig = {
  localAddress: 0,
  groupAddress: 0,
  service: {
    uuid: '6e400001-b5a3-f393-e0a9-e50e24dcca9e',
    txCharacteristic: '6e400002-b5a3-f393-e0a9-e50e24dcca9e',
    rxCharacteristic: '6e400003-b5a3-f393-e0a9-e50e24dcca9e'
  },
  connection: {
    timeout: 10000,
    mtu: 20,
    reconnectAttempts: 3
  }
};
```

**Features:**
- ✅ Web Bluetooth API integration
- ✅ Nordic UART Service support  
- ✅ Automatic MTU handling and chunking
- ✅ Device persistence and reconnection
- ✅ Browser compatibility detection
- ✅ Secure context requirements (HTTPS)

### Planned Transports

| Transport | Status | Use Case |
|-----------|--------|----------|
| **WebSerial** | 🚧 Planned | Browser USB/serial connections |
| **Serial** | 🚧 Planned | Node.js serial port communication |
| **WebSocket** | 🚧 Planned | Network-based messaging |
| **TCP** | 🚧 Planned | Direct TCP connections |
| **Mock** | 🚧 Planned | Testing without hardware |

## TypeScript Features

### Generic Type Safety

EventMsgV3 provides complete type safety for your message data:

```typescript
// Define your message types
interface SensorReading {
  temperature: number;
  humidity: number;
  timestamp: number;
}

interface DeviceCommand {
  action: 'led' | 'relay' | 'motor';
  state: boolean;
  intensity?: number;
}

// Type-safe sending
await eventMsg.send<DeviceCommand>('device_control', {
  action: 'led',
  state: true,
  intensity: 80
}, { receiverId: 1 });

// Type-safe receiving
eventMsg.onMessage<SensorReading>('sensor_data', (data, metadata) => {
  // TypeScript knows data has temperature, humidity, timestamp
  console.log(`${data.temperature}°C, ${data.humidity}% from device ${metadata.senderId}`);
});

// Type-safe waiting
const reading = await eventMsg.waitFor<SensorReading>('sensor_data');
console.log(`Current temp: ${reading.data.temperature}°C`);
```

### Interface Definitions

All public interfaces are exported for your use:

```typescript
import type {
  EventMsgConfig,
  MessageMetadata,
  MessageResult,
  SendOptions,
  WaitForOptions,
  ConnectionStats
} from '@eventmsg/core';

import type {
  WebBLETransportConfig,
  NordicUARTService
} from '@eventmsg/transport-webble';
```

### Strict Error Typing

Comprehensive error types help you handle failures properly:

```typescript
import {
  ConnectionError,
  SendError, 
  WaitForTimeoutError,
  AddressValidationError
} from '@eventmsg/core';

try {
  await eventMsg.send('test', {}, { receiverId: 999 }); // Invalid address
} catch (error) {
  if (error instanceof AddressValidationError) {
    console.error(`Invalid address: ${error.value}`);
  } else if (error instanceof ConnectionError) {
    console.error('Not connected to device');
  }
}
```

## Advanced Usage

### Custom Message Filtering

```typescript
// Wait for sensor data from specific device in specific group
const sensorData = await eventMsg.waitFor('sensor_data', {
  timeout: 10000,
  filter: (metadata) => 
    metadata.senderId === 5 && 
    metadata.senderGroupId === 1 &&
    metadata.flags === 0x80 // Priority flag
});
```

### Broadcast Patterns

```typescript
// Send to all devices
await eventMsg.send('system_announcement', {
  message: 'System maintenance in 5 minutes',
  severity: 'warning'
}, {
  receiverId: 255,        // Broadcast to all devices
  receiverGroupId: 255    // All groups
});

// Send to specific group
await eventMsg.send('group_command', {
  action: 'activate_sequence_1'
}, {
  receiverId: 255,        // All devices  
  receiverGroupId: 1      // Only group 1
});
```

### Connection Monitoring

```typescript
// Monitor connection statistics
const stats = eventMsg.getStats();
console.log({
  messagesSent: stats.messagesSent,
  messagesReceived: stats.messagesReceived, 
  uptime: stats.uptime,
  connected: stats.connected
});

// Handle lifecycle events
eventMsg.on('connect', () => {
  console.log('🔗 Connected to device');
});

eventMsg.on('disconnect', () => {
  console.log('🔌 Disconnected from device');
});

eventMsg.on('error', (error) => {
  console.error('💥 Transport error:', error.message);
});
```

### Debug Mode

```typescript
// Enable detailed logging
const eventMsg = new EventMsg({ 
  transport, 
  debug: true 
});

// See hex dumps of messages
// Console output:
// [EventMsg] Encoded message: 01 00 01 00 00 00 00 01 02 70 69 6e 67 1f 48 65 6c 6c 6f 04
// [EventMsg] Message sent: ping -> Hello
// [EventMsg] Message received: pong from device 1
```

## Examples

### Ping/Pong with RTT Measurement

```typescript
async function measureLatency() {
  const startTime = Date.now();
  
  // Send ping
  await eventMsg.send('ping', { timestamp: startTime }, {
    receiverId: 1
  });
  
  // Wait for pong
  const pong = await eventMsg.waitFor('pong', {
    timeout: 5000,
    filter: (meta) => meta.senderId === 1
  });
  
  const rtt = Date.now() - startTime;
  console.log(`Round-trip time: ${rtt}ms`);
}
```

### Sensor Data Collection

```typescript
interface SensorReading {
  temperature: number;
  humidity: number;
  pressure: number;
  timestamp: number;
}

// Request sensor data
await eventMsg.send('get_sensors', {}, { receiverId: 1 });

// Collect responses
const readings: SensorReading[] = [];

eventMsg.onMessage<SensorReading>('sensor_data', (data, metadata) => {
  readings.push(data);
  console.log(`📊 Device ${metadata.senderId}: ${data.temperature}°C`);
});
```

### Device Configuration

```typescript
interface DeviceConfig {
  wifiSSID?: string;
  sampleRate?: number;
  enabledSensors?: string[];
}

async function configureDevice(deviceId: number, config: DeviceConfig) {
  try {
    // Send configuration
    await eventMsg.send('configure', config, {
      receiverId: deviceId,
      flags: 0x80 // Priority flag
    });
    
    // Wait for acknowledgment
    await eventMsg.waitFor('config_ack', {
      timeout: 10000,
      filter: (meta) => meta.senderId === deviceId
    });
    
    console.log(`✅ Device ${deviceId} configured successfully`);
  } catch (error) {
    console.error(`❌ Configuration failed: ${error.message}`);
  }
}
```

### Multi-Device Coordination

```typescript
// Send command to device group
await eventMsg.send('start_recording', {
  duration: 60000,
  format: 'csv'
}, {
  receiverId: 255,      // All devices
  receiverGroupId: 1    // Group 1 only
});

// Collect results from all devices in group
const results = new Map();

eventMsg.onMessage('recording_complete', (data, metadata) => {
  if (metadata.senderGroupId === 1) {
    results.set(metadata.senderId, data);
    console.log(`📁 Device ${metadata.senderId} finished recording`);
  }
});
```

## Protocol Details

<details>
<summary><strong>Binary Protocol Specification</strong></summary>

### Message Format

```
[SOH][Stuffed Header][STX][Stuffed Event Name][US][Stuffed Event Data][EOT]
```

### Header Structure (7 bytes)
```
Byte 0: Sender ID (0-255)
Byte 1: Receiver ID (0-255)  
Byte 2: Sender Group ID (0-255)
Byte 3: Receiver Group ID (0-255)
Byte 4: Flags (0-255)
Bytes 5-6: Message ID (0-65535, big-endian)
```

### Control Characters
- `SOH` (0x01): Start of Header
- `STX` (0x02): Start of Text  
- `US` (0x1F): Unit Separator
- `EOT` (0x04): End of Transmission
- `ESC` (0x1B): Escape Character

### Byte Stuffing
Control characters in data are escaped as: `ESC + (char XOR 0x20)`

### Size Limits
- Event Name: 64 bytes maximum
- Event Data: 3,048 bytes maximum  
- Total Message: 4,096 bytes maximum (after stuffing)

</details>

<details>
<summary><strong>Addressing Examples</strong></summary>

### Device-to-Device (Unicast)
```typescript
await eventMsg.send('message', data, {
  receiverId: 5,           // Send to device 5
  receiverGroupId: 0       // Default group
});
```

### Broadcast to All Devices
```typescript
await eventMsg.send('announcement', data, {
  receiverId: 255,         // Broadcast
  receiverGroupId: 255     // All groups
});
```

### Group Multicast  
```typescript
await eventMsg.send('group_command', data, {
  receiverId: 255,         // All devices
  receiverGroupId: 1       // Group 1 only
});
```

### Targeted Group Message
```typescript
await eventMsg.send('specific_task', data, {
  receiverId: 3,           // Device 3 specifically
  receiverGroupId: 1       // Must be in group 1
});
```

</details>

## Contributing

We welcome contributions! This is an open-source project built for the community.

### Development Setup

```bash
# Clone the repository
git clone https://github.com/your-org/eventmsgv3-ts.git
cd eventmsgv3-ts

# Install dependencies
bun install

# Run tests
bun test

# Build packages
bun run build

# Run examples
cd packages/transport-webble/examples
# Open basic-test.html in browser
```

### Project Structure

```
eventmsgv3-ts/
├── packages/
│   ├── core/                 # Core EventMsg library
│   ├── transport-webble/     # Web Bluetooth transport
│   ├── transport-serial/     # Node.js serial (planned)
│   └── transport-websocket/  # WebSocket transport (planned)
├── examples/                 # Usage examples
├── docs/                     # Documentation
└── tools/                    # Build and development tools
```

### License

MIT License - see [LICENSE](LICENSE) for details.

---

<div align="center">

**[Documentation](docs/) • [Examples](examples/) • [Issues](https://github.com/your-org/eventmsgv3-ts/issues) • [Discussions](https://github.com/your-org/eventmsgv3-ts/discussions)**

Made with ❤️ for the ESP32 and web development community

</div>