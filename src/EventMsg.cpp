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

bool EventMsg::process(uint8_t* data, size_t len) {
    if(len < 3) {
        DEBUG_PRINT("Error: Message too short (%d bytes)", len);
        return false;
    }

    uint8_t tempBuf[MAX_EVENT_DATA_SIZE];
    uint8_t header[MAX_HEADER_SIZE];
    char eventName[MAX_EVENT_NAME_SIZE + 1];
    char eventData[MAX_EVENT_DATA_SIZE + 1];

    size_t pos = 0;
    
    // Verify SOH
    if(data[pos++] != SOH) {
        DEBUG_PRINT("Error: Invalid SOH");
        return false;
    }

    // Find STX
    size_t headerEnd = 1;
    while(headerEnd < len && data[headerEnd] != STX) headerEnd++;
    if(headerEnd >= len) {
        DEBUG_PRINT("Error: STX not found");
        return false;
    }

    // Process header
    size_t headerLen = ByteUnstuff(&data[1], headerEnd - 1, header, sizeof(header));
    if(headerLen != MAX_HEADER_SIZE) {
        DEBUG_PRINT("Error: Invalid header length (%d bytes)", headerLen);
        return false;
    }

    // Extract header info
    uint8_t sender = header[0];
    uint8_t receiver = header[1];
    uint8_t group = header[2];
    uint8_t flags = header[3];
    uint16_t msgId = (header[4] << 8) | header[5];

    DEBUG_PRINT("Header: sender=0x%02X, receiver=0x%02X, group=0x%02X, flags=0x%02X, msgId=%u",
                sender, receiver, group, flags, msgId);

    // Skip STX
    pos = headerEnd + 1;

    // Find US
    size_t nameEnd = pos;
    while(nameEnd < len && data[nameEnd] != US) nameEnd++;
    if(nameEnd >= len) {
        DEBUG_PRINT("Error: US not found");
        return false;
    }

    // Process event name
    size_t nameLen = ByteUnstuff(&data[pos], nameEnd - pos, tempBuf, sizeof(tempBuf));
    if(nameLen >= MAX_EVENT_NAME_SIZE) {
        DEBUG_PRINT("Error: Event name too long (%d bytes)", nameLen);
        return false;
    }
    memcpy(eventName, tempBuf, nameLen);
    eventName[nameLen] = '\0';

    DEBUG_PRINT("Event Name: %s (%d bytes)", eventName, nameLen);

    // Skip US
    pos = nameEnd + 1;

    // Find EOT
    size_t dataEnd = pos;
    while(dataEnd < len && data[dataEnd] != EOT) dataEnd++;
    if(dataEnd >= len) {
        DEBUG_PRINT("Error: EOT not found");
        return false;
    }

    // Process event data
    size_t dataLen = ByteUnstuff(&data[pos], dataEnd - pos, tempBuf, sizeof(tempBuf));
    if(dataLen >= MAX_EVENT_DATA_SIZE) {
        DEBUG_PRINT("Error: Event data too long (%d bytes)", dataLen);
        return false;
    }
    memcpy(eventData, tempBuf, dataLen);
    eventData[dataLen] = '\0';

    DEBUG_PRINT("Event Data Length: %d bytes", dataLen);

    // Check if this message is for us
    if((receiver == localAddr || receiver == 0xFF) &&
       (group == groupAddr || group == 0)) {
        DEBUG_PRINT("Message accepted (matches our address/group)");
        // Call event handler if registered
        if(eventCallback) {
            eventCallback(eventName, eventData);
        }
        return true;
    }

    DEBUG_PRINT("Message ignored (not for us: addr=0x%02X, group=0x%02X)", localAddr, groupAddr);
    return false;
}
