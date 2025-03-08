#include "EventMsg.h"
#include <string.h>

bool EventMsg::init(WriteCallback cb) {
    setWriteCallback(cb);
    for(int i = 0; i < sourceStates.size(); i++) {
        resetState(i);
    }
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
    for (const auto& dispatcher : dispatchers) {
        if (dispatcher.deviceName == deviceName) {
            return false;
        }
    }

    EventDispatcherInfo dispatcher{
        std::string(deviceName),
        cb,
        header.receiverId,
        header.senderId,
        header.groupId
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
    for (const auto& handler : rawHandlers) {
        if (handler.deviceName == deviceName) {
            return false;
        }
    }
    
    RawDataHandler handler{
        std::string(deviceName),
        cb,
        header.receiverId,
        header.senderId,
        header.groupId
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
    unhandledHandler->receiverId = header.receiverId;
    unhandledHandler->senderId = header.senderId;
    unhandledHandler->groupId = header.groupId;
}

void EventMsg::processAllSources() {
    sourceManager.processAll([this](uint8_t sourceId, uint8_t* data, size_t length) {
        this->process(sourceId, data, length);
    });
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

size_t EventMsg::send(const char* name, const char* data, uint8_t receiverId, uint8_t groupId, uint8_t senderId) {
    EventHeader header = {
        senderId,
        receiverId,
        groupId,
        0x00
    };
    return send(name, data, header);
}

size_t EventMsg::send(const char* name, const char* data, uint8_t receiverId, uint8_t groupId=0x00) {
    EventHeader header = {
        localAddr,
        receiverId,
        groupId,
        0x00
    };
    return send(name, data, header);
}

size_t EventMsg::send(const char* name, const char* data, const EventHeader& header) {
    std::vector<uint8_t> tempBuf;
    std::vector<uint8_t> msgBuf;
    tempBuf.reserve(MAX_EVENT_NAME_SIZE * 2);
    msgBuf.reserve(MAX_EVENT_DATA_SIZE * 2);
    
    msgBuf.push_back(SOH);

    uint8_t headerBytes[] = {
        header.senderId,
        header.receiverId,
        header.groupId,
        header.flags,
        (uint8_t)(msgIdCounter >> 8),
        (uint8_t)(msgIdCounter & 0xFF)
    };
    msgIdCounter++;

    tempBuf.resize(MAX_EVENT_NAME_SIZE * 2);
    size_t stuffedLen = ByteStuff(headerBytes, sizeof(headerBytes), tempBuf.data(), tempBuf.size());
    if(stuffedLen == 0) return 0;
    
    msgBuf.insert(msgBuf.end(), tempBuf.begin(), tempBuf.begin() + stuffedLen);
    msgBuf.push_back(STX);

    std::vector<uint8_t> eventNameBytes(MAX_EVENT_NAME_SIZE);
    size_t eventNameLen = StringToBytes(name, eventNameBytes.data(), eventNameBytes.size());
    if(eventNameLen == 0) return 0;

    stuffedLen = ByteStuff(eventNameBytes.data(), eventNameLen, tempBuf.data(), tempBuf.size());
    if(stuffedLen == 0) return 0;
    
    msgBuf.insert(msgBuf.end(), tempBuf.begin(), tempBuf.begin() + stuffedLen);
    msgBuf.push_back(US);

    std::vector<uint8_t> eventDataBytes(MAX_EVENT_DATA_SIZE);
    size_t eventDataLen = StringToBytes(data, eventDataBytes.data(), eventDataBytes.size());
    if(eventDataLen == 0) return 0;

    tempBuf.resize(msgBuf.capacity() - msgBuf.size() - 1);
    stuffedLen = ByteStuff(eventDataBytes.data(), eventDataLen, tempBuf.data(), tempBuf.size());
    if(stuffedLen == 0) return 0;
    
    msgBuf.insert(msgBuf.end(), tempBuf.begin(), tempBuf.begin() + stuffedLen);
    msgBuf.push_back(EOT);

    if(writeCallback) {
        if(writeCallback(msgBuf.data(), msgBuf.size())) {
            return msgBuf.size();
        }
    }
    return 0;
}

void EventMsg::resetState(uint8_t sourceId) {
    auto& state = sourceStates[sourceId];
    state.state = ProcessState::WAITING_FOR_SOH;
    state.currentBuffer = nullptr;
    state.bufferPos = 0;
    state.escapedMode = false;
    state.headerBuffer.clear();
    state.eventNameBuffer.clear();
    state.eventDataBuffer.clear();
}

bool EventMsg::isHandlerMatch(const EventHeader& header, uint8_t receiverId, uint8_t senderId, uint8_t groupId) {
    if (receiverId != BROADCAST_ADDR && 
        header.receiverId != BROADCAST_ADDR && 
        receiverId != header.receiverId) {
        return false;
    }
    
    if (groupId != BROADCAST_ADDR &&
        header.groupId != BROADCAST_ADDR &&
        groupId != header.groupId) {
        return false;
    }
    
    return true;
}

void EventMsg::processCallbacks(const char* eventName, const uint8_t* data, size_t length, EventHeader& header) {
    bool eventHandled = false;

    for (const auto& handler : rawHandlers) {
        if (handler.callback && isHandlerMatch(header, handler.receiverId, handler.senderId, handler.groupId)) {
            handler.callback(handler.deviceName.c_str(), data, length);
        }
    }

    for (const auto& dispatcher : dispatchers) {
        if (dispatcher.callback && isHandlerMatch(header, dispatcher.receiverId, dispatcher.senderId, dispatcher.groupId)) {
            dispatcher.callback(dispatcher.deviceName.c_str(), 
                             eventName,
                             (const char*)data,
                             header);
            eventHandled = true;
        }
    }

    if (!eventHandled && unhandledHandler && unhandledHandler->callback &&
        isHandlerMatch(header, unhandledHandler->receiverId, unhandledHandler->senderId, unhandledHandler->groupId)) {
        unhandledHandler->callback(unhandledHandler->deviceName.c_str(),
                                 eventName,
                                 (const char*)data,
                                 header);
    }
}

bool EventMsg::processNextByte(uint8_t sourceId, uint8_t byte) {
    auto& state = sourceStates[sourceId];
    
    if (state.escapedMode) {
        byte ^= 0x20;
        state.escapedMode = false;
    } else if (byte == ESC) {
        state.escapedMode = true;
        return true;
    }
    
    switch (state.state) {
        case ProcessState::WAITING_FOR_SOH:
            if (byte == SOH) {
                state.state = ProcessState::READING_HEADER;
                state.headerBuffer.clear();
                state.bufferPos = 0;
            }
            break;

        case ProcessState::READING_HEADER:
            state.headerBuffer.push_back(byte);
            state.bufferPos++;
            if (state.bufferPos == MAX_HEADER_SIZE) {
                // Extract header info
                uint8_t sender = state.headerBuffer[0];
                uint8_t receiver = state.headerBuffer[1];
                uint8_t group = state.headerBuffer[2];
                uint8_t flags = state.headerBuffer[3];
                uint16_t msgId = (state.headerBuffer[4] << 8) | state.headerBuffer[5];

                DEBUG_PRINT("Header: sender=0x%02X, receiver=0x%02X, group=0x%02X, flags=0x%02X, msgId=%u",
                           sender, receiver, group, flags, msgId);

                state.state = ProcessState::WAITING_FOR_STX;
            }
            break;

        case ProcessState::WAITING_FOR_STX:
            if (byte == STX) {
                state.state = ProcessState::READING_EVENT_NAME;
                state.eventNameBuffer.clear();
                state.bufferPos = 0;
            } else {
                return false;
            }
            break;

        case ProcessState::READING_EVENT_NAME:
            if (byte == US) {
                state.eventNameBuffer.push_back('\0');
                DEBUG_PRINT("Event Name: %s (%d bytes)", state.eventNameBuffer.data(), state.bufferPos);
                
                state.state = ProcessState::READING_EVENT_DATA;
                state.eventDataBuffer.clear();
                state.bufferPos = 0;
            } else {
                if (state.bufferPos >= MAX_EVENT_NAME_SIZE) {
                    return false;
                }
                state.eventNameBuffer.push_back(byte);
                state.bufferPos++;
            }
            break;

        case ProcessState::READING_EVENT_DATA:
            if (byte == EOT) {
                state.eventDataBuffer.push_back('\0');
                
                EventHeader msgHeader = {
                    state.headerBuffer[0],
                    state.headerBuffer[1],
                    state.headerBuffer[2],
                    state.headerBuffer[3]
                };

                DEBUG_PRINT("Event Data: (%d bytes)", state.bufferPos);
                processCallbacks((const char*)state.eventNameBuffer.data(),
                               state.eventDataBuffer.data(),
                               state.bufferPos,
                               msgHeader);
                
                resetState(sourceId);
            } else {
                if (state.bufferPos >= MAX_EVENT_DATA_SIZE) {
                    return false;
                }
                state.eventDataBuffer.push_back(byte);
                state.bufferPos++;
            }
            break;
    }
    
    return true;
}

bool EventMsg::process(uint8_t sourceId, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (!processNextByte(sourceId, data[i])) {
            resetState(sourceId);
            return false;
        }
    }
    return true;
}
