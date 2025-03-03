# Basic EventMsg Example

This example demonstrates the core functionality of the EventMsg library using Serial communication on an ESP32.

## Features

- LED control via events
- Heartbeat messages
- Ping/Pong functionality
- Event acknowledgments

## Hardware Required

- ESP32 development board
- USB cable
- Built-in LED (or external LED connected to GPIO 2)

## Installation

1. Open this example in PlatformIO
2. Connect your ESP32 board
3. Build and upload the code

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

2. Ping Test:
   ```
   PING with any data
   ```
   - Response: `PONG` event with the same data echoed back

### Automatic Messages

The device automatically sends:
- `HEARTBEAT` event every 5 seconds with uptime information

## Example Serial Output

When you start the device, you'll see:
```
EventMsg Demo Ready!
Commands:
1. LED_CONTROL with data '1' or '0'
2. PING with any data
```

## Protocol Details

Messages follow the EventMsg protocol format:
```
[SOH][Header][STX][Event Name][US][Event Data][EOT]
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

## Next Steps

1. Try modifying the example to:
   - Add new event types
   - Change heartbeat interval
   - Add more sensors or outputs

2. Explore the [ESP32 BLE example](../ESP32_BLE) for wireless communication
