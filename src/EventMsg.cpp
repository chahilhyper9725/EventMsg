#include "EventMsg.h"
#include <string.h>

bool EventMsg::init(WriteCallback cb) {
    writeCallback = cb;
    resetState();
    unhandledHandler = nullptr;
    return true;
}

void EventMsg::setAddr(uint8_t addr) {
    localAddr = addr;
}

void EventMsg::setGroup(uint8_t addr) {
    groupAddr = addr;
}

bool EventMsg::registerDispatcher(const char* deviceName, const EventHeader& header, EventDispatcherCallback cb) {
    // Check if dispatcher already exists
    for (const auto& dispatcher : dispatchers) {
        if (dispatcher.deviceName == deviceName) {
            return false; // Dispatcher already exists
        }
    }

    // Create new dispatcher
    EventDispatcherInfo dispatcher{
        std::string(deviceName),
        cb
    };
    dispatchers.push_back(dispatcher);
    return true;
}

bool EventMsg::unregisterDispatcher(const char* deviceName) {
    for (auto it = dispatchers.begin(); it != dispatchers.end(); ++it) {
        if (it->deviceName == deviceName) {
            dispatchers.erase(it);
            return true;
        }
    }
    return false;
}

bool EventMsg::registerRawHandler(const char* deviceName, const EventHeader& header, RawDataCallback cb) {
    // Check if handler already exists
    for (const auto& handler : rawHandlers) {
        if (handler.deviceName == deviceName) {
            return false; // Handler already exists
        }
    }
    
    // Create new handler
    RawDataHandler handler{
        std::string(deviceName),
        cb
    };
    rawHandlers.push_back(handler);
    return true;
}

bool EventMsg::unregisterRawHandler(const char* deviceName) {
    for (auto it = rawHandlers.begin(); it != rawHandlers.end(); ++it) {
        if (it->deviceName == deviceName) {
            rawHandlers.erase(it);
            return true;
        }
    }
    return false;
}

void EventMsg::setUnhandledHandler(const char* deviceName, const EventHeader& header, EventDispatcherCallback cb) {
    if (unhandledHandler == nullptr) {
        unhandledHandler = new EventDispatcherInfo;
    }
    unhandledHandler->deviceName = std::string(deviceName);
    unhandledHandler->callback = cb;
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

size_t EventMsg::send(const char* name, const char* data, const EventHeader& header) {
    uint8_t tempBuf[MAX_EVENT_NAME_SIZE * 2];
    uint8_t msgBuf[MAX_EVENT_DATA_SIZE * 2];
    size_t pos = 0;

    // Start message
    msgBuf[pos++] = SOH;

    // Create and stuff header bytes
    uint8_t headerBytes[] = {
        localAddr,                // Local address as sender
        header.receiverId,        // Destination address
        header.groupId,          // Group address
        header.flags,            // Flags
        (uint8_t)(msgIdCounter >> 8),
        (uint8_t)(msgIdCounter & 0xFF)
    };
    msgIdCounter++;

    size_t stuffedLen = ByteStuff(headerBytes, sizeof(headerBytes), tempBuf, sizeof(tempBuf));
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
    currentBuffer = nullptr;
    currentMaxLength = 0;
    bufferPos = 0;
    escapedMode = false;
    
    // Clear all buffers
    memset(headerBuffer, 0, sizeof(headerBuffer));
    memset(eventNameBuffer, 0, sizeof(eventNameBuffer));
    memset(eventDataBuffer, 0, sizeof(eventDataBuffer));
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
                currentBuffer = headerBuffer;
                currentMaxLength = MAX_HEADER_SIZE;
                bufferPos = 0;
            }
            break;

        case ProcessState::READING_HEADER:
            currentBuffer[bufferPos++] = byte;
            if (bufferPos == currentMaxLength) {
                // Extract header info
                uint8_t sender = headerBuffer[0];
                uint8_t receiver = headerBuffer[1];
                uint8_t group = headerBuffer[2];
                uint8_t flags = headerBuffer[3];
                uint16_t msgId = (headerBuffer[4] << 8) | headerBuffer[5];

                DEBUG_PRINT("Header: sender=0x%02X, receiver=0x%02X, group=0x%02X, flags=0x%02X, msgId=%u",
                           sender, receiver, group, flags, msgId);

                state = ProcessState::WAITING_FOR_STX;
            }
            break;

        case ProcessState::WAITING_FOR_STX:
            if (byte == STX) {
                state = ProcessState::READING_EVENT_NAME;
                currentBuffer = eventNameBuffer;
                currentMaxLength = MAX_EVENT_NAME_SIZE;
                bufferPos = 0;
            } else {
                return false; // Invalid sequence
            }
            break;

        case ProcessState::READING_EVENT_NAME:
            if (byte == US) {
                // Null terminate event name
                currentBuffer[bufferPos] = '\0';
                DEBUG_PRINT("Event Name: %s (%d bytes)", currentBuffer, bufferPos);
                
                state = ProcessState::READING_EVENT_DATA;
                currentBuffer = eventDataBuffer;
                currentMaxLength = MAX_EVENT_DATA_SIZE;
                bufferPos = 0;
            } else {
                if (bufferPos >= currentMaxLength) {
                    return false; // Name too long
                }
                currentBuffer[bufferPos++] = byte;
            }
            break;

        case ProcessState::READING_EVENT_DATA:
            if (byte == EOT) {
                // Message complete, process it
                currentBuffer[bufferPos] = '\0';
                
                // Create EventHeader from received data
                EventHeader msgHeader = {
                    headerBuffer[0], // sender
                    headerBuffer[1], // receiver
                    headerBuffer[2], // group
                    headerBuffer[3]  // flags
                };
                
                bool eventHandled = false;

                // Call raw data handlers first
                for (const auto& handler : rawHandlers) {
                    if (handler.callback) {
                        handler.callback(handler.deviceName.c_str(),
                                      (const char*)eventNameBuffer,
                                      currentBuffer,
                                      bufferPos,
                                      msgHeader);
                    }
                }

                // Call dispatchers
                for (const auto& dispatcher : dispatchers) {
                    if (dispatcher.callback) {
                        dispatcher.callback(dispatcher.deviceName.c_str(),
                                         (const char*)eventNameBuffer,
                                         (const char*)eventDataBuffer,
                                         msgHeader);
                        eventHandled = true;
                    }
                }

                // Call unhandled handler if no dispatcher handled the event
                if (!eventHandled && unhandledHandler && unhandledHandler->callback) {
                    unhandledHandler->callback(unhandledHandler->deviceName.c_str(),
                                            (const char*)eventNameBuffer,
                                            (const char*)eventDataBuffer,
                                            msgHeader);
                }
                
                // Reset for next message
                resetState();
            } else {
                if (bufferPos >= currentMaxLength) {
                    return false; // Data too long
                }
                currentBuffer[bufferPos++] = byte;
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
