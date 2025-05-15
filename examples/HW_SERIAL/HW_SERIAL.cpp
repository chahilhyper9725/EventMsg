
#include <Arduino.h>
#include <EventMsg.h>
#include <EventDispatcher.h>
#include <HardwareSerial.h>

HardwareSerial sensorSerial(1);
#define RX_PIN 16
#define TX_PIN 17

EventMsg eventMsg;
EventDispatcher sensorDispatcher(0xFF, 0xFF, 0xFF); // Mobile app messages
int sensorSerialId;

bool serialWrite(uint8_t *data, size_t len)
{
    sensorSerial.write(data, len);
    sensorSerial.flush();
    return true;
}

void ledon(const char *data, size_t length, EventHeader &header)
{
    Serial.printf("LED ON command received: %s (length: %d)\n", data, length);
}

void setup()
{
    Serial.begin(115200);
    eventMsg.createSource(1, 16);
    eventMsg.createSource(1, 16);
    sensorSerialId = eventMsg.createSource(1024, 16);
    sensorSerial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

    // Check if PSRAM is enabled
    if (EventMsg::isPSRAMEnabled())
    {
        Serial.println("PSRAM support is enabled");
    }
    else
    {
        Serial.println("PSRAM support is not enabled");
    }

    eventMsg.setWriteCallback(serialWrite);

    sensorDispatcher.registerWith(eventMsg, "sensordata");

    sensorDispatcher.on("sensordata", [](const char *data, size_t length, EventHeader &header)
                        { Serial.printf("Received sensor data: %s (length: %d)\n", data, length); });

sensorDispatcher.on("ledon",ledon);

    eventMsg.setUnhandledHandler("unhandled", sensorDispatcher.getListenHeader(), [](const char *deviceName, const char *eventName, const char *data, size_t length, EventHeader &header)
                                 { Serial.printf("Unhandled event: %s, data: %s (length: %d)\n", eventName, data, length); });
}

long lastMillis = 0;

void loop()
{
    eventMsg.processAllSources();

    if (sensorSerial.available() > 0)
    {
        uint8_t buffer[256];
        size_t len = sensorSerial.readBytes(buffer, sizeof(buffer));
        Serial.printf("Received %d bytes from sensor serial\n", len);
        bool queued = sourceManager.pushToSource(
            sensorSerialId,
            (uint8_t *)buffer,
            len);
    }

    if (Serial.available())
    {
        char data = Serial.read();

        if (data == 's')
        {
            String eventname = Serial.readStringUntil('#');
            String data = Serial.readStringUntil('\n');
            Serial.println(data);
            eventMsg.send(eventname.c_str(), data.c_str(), sensorDispatcher.createHeader(0xFF, 0x01));
        }
    }

    if (millis() - lastMillis > 1000)
    {
        static int count = 0;
        count++;
        String eventname = "sensordata";
        String data = String(count);
        lastMillis = millis();
        // Send the event with the updated count
        eventMsg.send(eventname.c_str(), data.c_str(), sensorDispatcher.createHeader(0xFF, 0x01));
    }
}
