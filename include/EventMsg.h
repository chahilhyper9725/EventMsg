#ifndef EVENT_MSG_H
#define EVENT_MSG_H

#include <Arduino.h>
#include <functional>

// Protocol Control Characters
#define SOH 0x01  // Start of Header
#define STX 0x02  // Start of Text
#define US  0x1F  // Unit Separator
#define EOT 0x04  // End of Transmission
#define ESC 0x1B  // Escape Character

// Protocol Size Definitions
#define MAX_HEADER_SIZE    6      // Fixed header size (sender,receiver,group,flags,msgid)
#define MAX_EVENT_NAME_SIZE 32    // Maximum raw event name length
#define MAX_EVENT_DATA_SIZE 2048  // Maximum raw event data length

// Enhanced debug output with timestamp and module name
#define DEBUG_PRINT(msg, ...) \
    do { \
        Serial.printf("[%lu][EventMsg] ", millis()); \
        Serial.printf(msg "\n", ##__VA_ARGS__); \
    } while(0)

// Function type for data transmission
using WriteCallback = std::function<bool(uint8_t*, size_t)>;

// Function type for event handling
using EventCallback = std::function<void(const char*, const char*)>;

class EventMsg {
private:
    // Message assembly state machine
    enum class ProcessState {
        WAITING_FOR_SOH,
        READING_HEADER,
        WAITING_FOR_STX,
        READING_EVENT_NAME,
        WAITING_FOR_US,
        READING_EVENT_DATA,
        WAITING_FOR_EOT
    };

    // Configuration
    uint8_t localAddr;
    uint8_t groupAddr;
    uint16_t msgIdCounter;
    WriteCallback writeCallback;
    EventCallback eventCallback;
    uint8_t receiverId;
    uint8_t groupId;

    // State machine buffers and tracking
    ProcessState state;
    uint8_t assemblyBuffer[MAX_EVENT_DATA_SIZE];
    size_t bufferPos;
    size_t expectedLength;
    bool escapedMode;

    // Internal methods
    bool processNextByte(uint8_t byte);
    void resetState();
    size_t ByteStuff(const uint8_t* input, size_t inputLen, uint8_t* output, size_t outputMaxLen);
    size_t ByteUnstuff(const uint8_t* input, size_t inputLen, uint8_t* output, size_t outputMaxLen);
    size_t StringToBytes(const char* str, uint8_t* output, size_t outputMaxLen);

public:
    EventMsg() : localAddr(0), groupAddr(0), msgIdCounter(0), 
                receiverId(0xFF), groupId(0) {}

    // Core functions
    bool init(WriteCallback cb);
    void setAddr(uint8_t addr);
    void setGroup(uint8_t addr);
    
    // Message handling
    size_t send(const char* name, const char* data, uint8_t recvAddr, uint8_t groupAddr, uint8_t flags);
    bool process(uint8_t* data, size_t len);
    
    // Event registration
    void onEvent(EventCallback cb, uint8_t receiverId = 0xFF, uint8_t groupId = 0);
};

#endif // EVENT_MSG_H
