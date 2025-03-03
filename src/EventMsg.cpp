#include "EventMsg.h"
#include <string.h>

bool EventMsg::init(WriteCallback cb) {
    writeCallback = cb;
    return true;
}

void EventMsg::setAddr(uint8_t addr) {
    localAddr = addr;
}

void EventMsg::setGroup(uint8_t addr) {
    groupAddr = addr;
}

void EventMsg::onEvent(EventCallback cb, uint8_t recvId, uint8_t grpId) {
    eventCallback = cb;
    receiverId = recvId;
    groupId = grpId;
}

size_t EventMsg::ByteStuff(const uint8_t* input, size_t inputLen, uint8_t* output, size_t outputMaxLen) {
    size_t outputLen = 0;
    const uint8_t controlChars[] = {SOH, STX, US, EOT, ESC};
    const size_t numControlChars = sizeof(controlChars);

    for(size_t i = 0; i < inputLen; i++) {
        bool needsStuffing = false;
        for(size_t j = 0; j < numControlChars; j++) {
            if(input[i] == controlChars[j]) {
                needsStuffing = true;
                break;
            }
        }

        if(needsStuffing) {
            if(outputLen + 2 > outputMaxLen) return 0;
            output[outputLen++] = ESC;
            output[outputLen++] = input[i] ^ 0x20;
        } else {
            if(outputLen + 1 > outputMaxLen) return 0;
            output[outputLen++] = input[i];
        }
    }
    return outputLen;
}

size_t EventMsg::ByteUnstuff(const uint8_t* input, size_t inputLen, uint8_t* output, size_t outputMaxLen) {
    size_t outputLen = 0;
    bool escapedMode = false;

    for(size_t i = 0; i < inputLen; i++) {
        if(escapedMode) {
            if(outputLen >= outputMaxLen) return 0;
            output[outputLen++] = input[i] ^ 0x20;
            escapedMode = false;
        } else if(input[i] == ESC) {
            escapedMode = true;
        } else {
            if(outputLen >= outputMaxLen) return 0;
            output[outputLen++] = input[i];
        }
    }
    return outputLen;
}

size_t EventMsg::StringToBytes(const char* str, uint8_t* output, size_t outputMaxLen) {
    size_t len = strlen(str);
    if(len >= outputMaxLen) return 0;
    memcpy(output, str, len);
    return len;
}

size_t EventMsg::send(const char* name, const char* data, uint8_t recvAddr, uint8_t groupAddr, uint8_t flags) {
    uint8_t tempBuf[MAX_EVENT_NAME_SIZE * 2];
    uint8_t msgBuf[MAX_EVENT_DATA_SIZE * 2];
    size_t pos = 0;

    // Start message
    msgBuf[pos++] = SOH;

    // Create and stuff header
    uint8_t header[] = {
        localAddr,
        recvAddr,
        groupAddr,
        flags,
        (uint8_t)(msgIdCounter >> 8),
        (uint8_t)(msgIdCounter & 0xFF)
    };
    msgIdCounter++;

    size_t stuffedLen = ByteStuff(header, sizeof(header), tempBuf, sizeof(tempBuf));
    if(stuffedLen == 0) return 0;
    memcpy(&msgBuf[pos], tempBuf, stuffedLen);
    pos += stuffedLen;

    // Add STX
    msgBuf[pos++] = STX;

    // Add event name
    uint8_t eventNameBytes[MAX_EVENT_NAME_SIZE];
    size_t eventNameLen = StringToBytes(name, eventNameBytes, sizeof(eventNameBytes));
    if(eventNameLen == 0) return 0;

    stuffedLen = ByteStuff(eventNameBytes, eventNameLen, tempBuf, sizeof(tempBuf));
    if(stuffedLen == 0) return 0;
    memcpy(&msgBuf[pos], tempBuf, stuffedLen);
    pos += stuffedLen;

    // Add separator
    msgBuf[pos++] = US;

    // Add event data
    uint8_t eventDataBytes[MAX_EVENT_DATA_SIZE];
    size_t eventDataLen = StringToBytes(data, eventDataBytes, sizeof(eventDataBytes));
    if(eventDataLen == 0) return 0;

    stuffedLen = ByteStuff(eventDataBytes, eventDataLen, &msgBuf[pos], sizeof(msgBuf) - pos - 1);
    if(stuffedLen == 0) return 0;
    pos += stuffedLen;

    // End message
    msgBuf[pos++] = EOT;

    // Send via callback
    if(writeCallback) {
        if(writeCallback(msgBuf, pos)) {
            return pos;
        }
    }
    return 0;
}

void EventMsg::resetState() {
    state = ProcessState::WAITING_FOR_SOH;
    bufferPos = 0;
    expectedLength = 0;
    escapedMode = false;
    memset(assemblyBuffer, 0, sizeof(assemblyBuffer));
}

bool EventMsg::processNextByte(uint8_t byte) {
    // Handle escape sequences
    if (escapedMode) {
        byte ^= 0x20;
        escapedMode = false;
    } else if (byte == ESC) {
        escapedMode = true;
        return true;
    }

    // Process based on current state
    switch (state) {
        case ProcessState::WAITING_FOR_SOH:
            if (byte == SOH) {
                state = ProcessState::READING_HEADER;
                expectedLength = MAX_HEADER_SIZE;
                bufferPos = 0;
            }
            break;

        case ProcessState::READING_HEADER:
            assemblyBuffer[bufferPos++] = byte;
            if (bufferPos == expectedLength) {
                // Extract header info
                uint8_t sender = assemblyBuffer[0];
                uint8_t receiver = assemblyBuffer[1];
                uint8_t group = assemblyBuffer[2];
                uint8_t flags = assemblyBuffer[3];
                uint16_t msgId = (assemblyBuffer[4] << 8) | assemblyBuffer[5];

                DEBUG_PRINT("Header: sender=0x%02X, receiver=0x%02X, group=0x%02X, flags=0x%02X, msgId=%u",
                            sender, receiver, group, flags, msgId);

                state = ProcessState::WAITING_FOR_STX;
                bufferPos = 0;
            }
            break;

        case ProcessState::WAITING_FOR_STX:
            if (byte == STX) {
                state = ProcessState::READING_EVENT_NAME;
                expectedLength = MAX_EVENT_NAME_SIZE;
            } else {
                return false; // Invalid sequence
            }
            break;

        case ProcessState::READING_EVENT_NAME:
            if (byte == US) {
                // Null terminate event name
                assemblyBuffer[bufferPos] = '\0';
                DEBUG_PRINT("Event Name: %s (%d bytes)", assemblyBuffer, bufferPos);
                
                state = ProcessState::READING_EVENT_DATA;
                bufferPos = 0;
            } else {
                if (bufferPos >= expectedLength) {
                    return false; // Name too long
                }
                assemblyBuffer[bufferPos++] = byte;
            }
            break;

        case ProcessState::READING_EVENT_DATA:
            if (byte == EOT) {
                // Message complete, process it
                assemblyBuffer[bufferPos] = '\0';
                
                // Check if message is for us
                uint8_t receiver = assemblyBuffer[1];
                uint8_t group = assemblyBuffer[2];
                
                if ((receiver == localAddr || receiver == 0xFF) &&
                    (group == groupAddr || group == 0)) {
                    DEBUG_PRINT("Message accepted (matches our address/group)");
                    // Call event handler with name and data
                    if (eventCallback) {
                        const char* eventName = (const char*)assemblyBuffer;
                        const char* eventData = (const char*)&assemblyBuffer[strlen(eventName) + 1];
                        eventCallback(eventName, eventData);
                    }
                }
                
                // Reset for next message
                resetState();
                state = ProcessState::WAITING_FOR_SOH;
            } else {
                if (bufferPos >= MAX_EVENT_DATA_SIZE) {
                    return false; // Data too long
                }
                assemblyBuffer[bufferPos++] = byte;
            }
            break;
    }
    
    return true;
}

bool EventMsg::process(uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (!processNextByte(data[i])) {
            resetState();
            return false;
        }
    }
    return true;
}
