#ifndef EVENT_MSG_H
#define EVENT_MSG_H

#include <Arduino.h>
#include <functional>
#include <vector>
#include <array>
#include <map>
#include <string>

struct RawPacket {
    static const size_t MAX_SIZE = 512;
    uint8_t sourceId;     // Identify message source
    uint32_t timestamp;   // When packet was received
    uint8_t data[MAX_SIZE];  // Fixed size buffer
    size_t length;
};

class ThreadSafeQueue {
private:
    static const size_t QUEUE_SIZE = 8;
    mutable std::array<RawPacket, QUEUE_SIZE> buffer;
    mutable SemaphoreHandle_t mutex;  // Mutable to allow locking in const methods
    mutable size_t head = 0;          // Must be mutable for const methods
    mutable size_t tail = 0;
    mutable bool full = false;

public:
    ThreadSafeQueue() {
        mutex = xSemaphoreCreateMutex();
    }

    ~ThreadSafeQueue() {
        vSemaphoreDelete(mutex);
    }

    bool push(const uint8_t* data, size_t len, uint8_t sourceId) const {
        if (len > RawPacket::MAX_SIZE) return false;
        
        xSemaphoreTake(mutex, portMAX_DELAY);
        
        if (full) {
            xSemaphoreGive(mutex);
            return false;
        }

        RawPacket& packet = const_cast<RawPacket&>(buffer[tail]);
        packet.sourceId = sourceId;
        packet.timestamp = millis();
        packet.length = len;
        memcpy(packet.data, data, len);

        tail = (tail + 1) % QUEUE_SIZE;
        full = (tail == head);
        
        xSemaphoreGive(mutex);
        return true;
    }

    bool tryPop(RawPacket& packet) const {
        if (!xSemaphoreTake(mutex, 0)) {
            return false;
        }

        bool success = false;
        if (head != tail || full) {
            // Thread-safe read from const buffer
            packet = buffer[head];
            // These must be mutable since they affect internal state
            head = (head + 1) % QUEUE_SIZE;
            full = false;
            success = true;
            
            // Update approximate metrics
            if (success) {
                droppedPackets += packet.timestamp < lastProcessed ? 1 : 0;
                lastProcessed = packet.timestamp;
                processedPackets++;
            }
        }

        xSemaphoreGive(mutex);
        return success;
    }

    // Queue status methods - all const
    size_t size() const {
        xSemaphoreTake(mutex, portMAX_DELAY);
        size_t count = full ? QUEUE_SIZE : (tail + QUEUE_SIZE - head) % QUEUE_SIZE;
        xSemaphoreGive(mutex);
        return count;
    }

    size_t maxSize() const { return QUEUE_SIZE; }

    size_t getProcessedCount() const { return processedPackets; }
    size_t getDroppedCount() const { return droppedPackets; }
    uint32_t getLastProcessedTime() const { return lastProcessed; }

protected:
    mutable size_t processedPackets = 0;
    mutable size_t droppedPackets = 0;
    mutable uint32_t lastProcessed = 0;

    bool isEmpty() const {
        xSemaphoreTake(mutex, portMAX_DELAY);
        bool empty = (head == tail && !full);
        xSemaphoreGive(mutex);
        return empty;
    }
};

class SourceQueueManager {
public:
    struct SourceConfig {
        size_t bufferSize;
        size_t queueSize;
        SourceConfig(size_t b = 512, size_t q = 8) : bufferSize(b), queueSize(q) {}
    };

    struct Source {
        ThreadSafeQueue queue;
        SourceConfig config;
        Source() = default;
        Source(const SourceConfig& c) : config(c) {}
    };

    uint8_t createSource(size_t bufferSize = 512, size_t queueSize = 8) {
        uint8_t sourceId = nextSourceId++;
        sources[sourceId] = Source(SourceConfig(bufferSize, queueSize));
        return sourceId;
    }

    bool pushToSource(uint8_t sourceId, const uint8_t* data, size_t len) const {
        auto it = sources.find(sourceId);
        if (it == sources.end()) return false;
        return it->second.queue.push(data, len, sourceId);
    }

    template<typename ProcessFunc>
    void processAll(ProcessFunc&& func) const {
        for(const auto& [sourceId, source] : sources) {
            RawPacket packet;
            while(source.queue.tryPop(packet)) {
                func(sourceId, packet.data, packet.length);
            }
        }
    }

    bool hasSource(uint8_t sourceId) const {
        return sources.find(sourceId) != sources.end();
    }

private:
    mutable std::map<uint8_t, Source> sources;
    uint8_t nextSourceId = 1;
};

// Global source queue manager
inline SourceQueueManager sourceManager;

struct EventHeader {
    uint8_t senderId;
    uint8_t receiverId;
    uint8_t groupId;
    uint8_t flags;
};

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

// Broadcast definitions
#define BROADCAST_ADDR 0xFF    // For both receiver and group
#define BROADCAST_SENDER 0xFF  // Accept all senders

#if ENABLE_EVENT_DEBUG_LOGS
#define DEBUG_PRINT(msg, ...) \
    do { \
        Serial.printf("[%lu][EventMsg] ", millis()); \
        Serial.printf(msg "\n", ##__VA_ARGS__); \
    } while(0)
#else
#define DEBUG_PRINT(msg, ...) 
#endif

// Function type for data transmission
using WriteCallback = std::function<bool(uint8_t*, size_t)>;

// Function type for event handling with header
using EventDispatcherCallback = std::function<void(const char* deviceName, const char* eventName, const char* data, EventHeader& header)>;
// Function type for raw data handling (simplified)
using RawDataCallback = std::function<void(const char* deviceName, const uint8_t* data, size_t length)>;

// Handler structures now using EventHeader internally
struct RawDataHandler {
    std::string deviceName;
    RawDataCallback callback;
    uint8_t receiverId;  // FF = accept broadcast
    uint8_t senderId;    // FF = accept any sender
    uint8_t groupId;     // FF = accept broadcast groups
};

struct EventDispatcherInfo {
    std::string deviceName;
    EventDispatcherCallback callback;
    uint8_t receiverId;  // FF = accept broadcast
    uint8_t senderId;    // FF = accept any sender
    uint8_t groupId;     // FF = accept broadcast groups
};

class EventMsg {
public:
    uint8_t createSource(size_t bufferSize = 512, size_t queueSize = 8) {
        return sourceManager.createSource(bufferSize, queueSize);
    }

    void setWriteCallback(WriteCallback cb) {
        writeCallback = cb;
    }

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

    // Per-source state management
    struct ProcessingState {
        ProcessState state = ProcessState::WAITING_FOR_SOH;
        std::vector<uint8_t> headerBuffer;
        std::vector<uint8_t> eventNameBuffer;
        std::vector<uint8_t> eventDataBuffer;
        uint8_t* currentBuffer = nullptr;
        size_t bufferPos = 0;
        bool escapedMode = false;

        ProcessingState() {
            headerBuffer.reserve(MAX_HEADER_SIZE);
            eventNameBuffer.reserve(MAX_EVENT_NAME_SIZE);
            eventDataBuffer.reserve(MAX_EVENT_DATA_SIZE);
        }
    };

    // Configuration
    uint8_t localAddr;
    uint8_t groupAddr;
    uint16_t msgIdCounter;
    WriteCallback writeCallback;
    std::vector<EventDispatcherInfo> dispatchers;
    std::vector<RawDataHandler> rawHandlers;
    EventDispatcherInfo* unhandledHandler;

    // State machine per source
    std::array<ProcessingState, 4> sourceStates;

    // Internal methods
    bool processNextByte(uint8_t sourceId, uint8_t byte);
    void processCallbacks(const char* eventName, const uint8_t* data, size_t length, EventHeader& header);
    void resetState(uint8_t sourceId);
    size_t ByteStuff(const uint8_t* input, size_t inputLen, uint8_t* output, size_t outputMaxLen);
    size_t ByteUnstuff(const uint8_t* input, size_t inputLen, uint8_t* output, size_t outputMaxLen);
    size_t StringToBytes(const char* str, uint8_t* output, size_t outputMaxLen);

public:
    EventMsg() : localAddr(0), groupAddr(0), msgIdCounter(0), unhandledHandler(nullptr) {}

    bool init(WriteCallback cb);
    void setAddr(uint8_t addr);
    void setGroup(uint8_t addr);
    bool isHandlerMatch(const EventHeader& header, uint8_t receiverId, uint8_t senderId, uint8_t groupId);

    void processAllSources();

    size_t send(const char* name, const char* data, const EventHeader& header);
    size_t send(const char* name, const char* data, uint8_t receiverId, uint8_t groupId, uint8_t senderId);
    size_t send(const char* name, const char* data, uint8_t receiverId, uint8_t groupId);
    bool process(uint8_t sourceId, const uint8_t* data, size_t len);
    
    // Event registration with simplified parameters
    bool registerRawHandler(const char* deviceName, const EventHeader& header, RawDataCallback cb);
    bool unregisterRawHandler(const char* deviceName);
    void setUnhandledHandler(const char* deviceName, const EventHeader& header, EventDispatcherCallback cb);
    
    // Dispatcher registration with simplified parameters
    bool registerDispatcher(const char* deviceName, const EventHeader& header, EventDispatcherCallback cb);
    bool unregisterDispatcher(const char* deviceName);
};

#endif // EVENT_MSG_H
