#ifndef EVENT_MSG_H
#define EVENT_MSG_H

#include <Arduino.h>
#include <functional>
#include <vector>
#include <array>
#include <map>
#include <string>
// Debug print macro definition
// #if ENABLE_EVENT_DEBUG_LOGS
// #define DEBUG_PRINT(msg, ...) \
//     do { \
//         Serial.printf("[%lu][EventMsg] ", millis()); \
//         Serial.printf(msg "\n", ##__VA_ARGS__); \
//     } while(0)
// #else
#define DEBUG_PRINT(msg, ...) 
// #endif

// PSRAM Support
#ifdef ESP32
#include <esp_heap_caps.h>

// Check if PSRAM is enabled via ESP-IDF config
#if CONFIG_SPIRAM_SUPPORT
#define EVENT_MSG_PSRAM_ENABLED 1
#else
#define EVENT_MSG_PSRAM_ENABLED 0
#endif

// Simple PSRAM allocation macros
#define EVENT_MSG_MALLOC(size) \
    (EVENT_MSG_PSRAM_ENABLED ? heap_caps_malloc(size, MALLOC_CAP_SPIRAM) : malloc(size))

#define EVENT_MSG_FREE(ptr) free(ptr)

#else
// For non-ESP32 platforms
#define EVENT_MSG_PSRAM_ENABLED 0
#define EVENT_MSG_MALLOC(size) malloc(size)
#define EVENT_MSG_FREE(ptr) free(ptr)
#endif

// Custom allocator for std::vector that uses PSRAM when available
template <typename T>
class PSRAMAllocator {
public:
    using value_type = T;
    
    PSRAMAllocator() = default;
    template <typename U> PSRAMAllocator(const PSRAMAllocator<U>&) {}
    
    T* allocate(std::size_t n) {
#if EVENT_MSG_PSRAM_ENABLED
        if (void* ptr = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM)) {
            return static_cast<T*>(ptr);
        }
#endif
        return static_cast<T*>(malloc(n * sizeof(T)));
    }
    
    void deallocate(T* p, std::size_t) {
        free(p);
    }
};

template <typename T, typename U>
bool operator==(const PSRAMAllocator<T>&, const PSRAMAllocator<U>&) { return true; }

template <typename T, typename U>
bool operator!=(const PSRAMAllocator<T>&, const PSRAMAllocator<U>&) { return false; }

// Define vector types that use PSRAM when available
template <typename T>
using PSRAMVector = std::vector<T, PSRAMAllocator<T>>;

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
    mutable bool initialized = false;

public:
    ThreadSafeQueue() : mutex(nullptr) {
        initialize();
    }

    ~ThreadSafeQueue() {
        if (mutex != nullptr) {
            vSemaphoreDelete(mutex);
            mutex = nullptr;
        }
    }
    
    void initialize() {
        // Use a critical section to ensure thread-safe initialization
        portENTER_CRITICAL(&mux);
        if (!initialized) {
            if (mutex == nullptr) {
                mutex = xSemaphoreCreateMutex();
                if (mutex != nullptr) {
                    initialized = true;
                    DEBUG_PRINT("ThreadSafeQueue mutex initialized successfully");
                } else {
                    DEBUG_PRINT("Failed to create ThreadSafeQueue mutex");
                }
            }
        }
        portEXIT_CRITICAL(&mux);
    }

private:
    // Mutex for protecting initialization
    static portMUX_TYPE mux;

public:
    bool push(const uint8_t* data, size_t len, uint8_t sourceId) const {
        if (len > RawPacket::MAX_SIZE) return false;
        
        // Ensure mutex is initialized
        if (mutex == nullptr) {
            DEBUG_PRINT("ThreadSafeQueue::push - mutex not initialized");
            return false;
        }
        
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            DEBUG_PRINT("ThreadSafeQueue::push - failed to take mutex");
            return false;
        }
        
        bool success = false;
        if (!full) {
            RawPacket& packet = const_cast<RawPacket&>(buffer[tail]);
            packet.sourceId = sourceId;
            packet.timestamp = millis();
            packet.length = len;
            memcpy(packet.data, data, len);

            tail = (tail + 1) % QUEUE_SIZE;
            full = (tail == head);
            success = true;
        }
        
        xSemaphoreGive(mutex);
        return success;
    }

public:
    bool tryPop(RawPacket& packet) const {
        // Ensure mutex is initialized
        if (mutex == nullptr) {
            DEBUG_PRINT("ThreadSafeQueue::tryPop - mutex not initialized");
            return false;
        }
        
        // Try to take the mutex with a timeout
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            DEBUG_PRINT("ThreadSafeQueue::tryPop - failed to take mutex");
            return false;
        }

        bool success = false;
        if (head != tail || full) {
            // Thread-safe read from const buffer
            packet = buffer[head];
            head = (head + 1) % QUEUE_SIZE;
            full = false;
            success = true;
            
            // Update metrics
            droppedPackets += packet.timestamp < lastProcessed ? 1 : 0;
            lastProcessed = packet.timestamp;
            processedPackets++;
        }

        xSemaphoreGive(mutex);
        return success;
    }

private:
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
        Source(const SourceConfig& c) : config(c) {
            // Ensure queue is initialized
            queue.initialize();
        }
    };

    uint8_t createSource(size_t bufferSize = 512, size_t queueSize = 8) {
        uint8_t sourceId = nextSourceId++;
        sources[sourceId] = Source(SourceConfig(bufferSize, queueSize));
        DEBUG_PRINT("Created source ID %d with buffer size %d and queue size %d", 
                   sourceId, bufferSize, queueSize);
        return sourceId;
    }

    bool pushToSource(uint8_t sourceId, const uint8_t* data, size_t len) const {
        auto it = sources.find(sourceId);
        if (it == sources.end()) {
            DEBUG_PRINT("pushToSource: Source ID %d not found", sourceId);
            return false;
        }
        return it->second.queue.push(data, len, sourceId);
    }

    template<typename ProcessFunc>
    void processAll(ProcessFunc&& func) const {
        // If no sources, return early
        if (sources.empty()) {
            DEBUG_PRINT("processAll: No sources to process");
            return;
        }
        
        // Avoid using structured bindings (C++17 feature)
        for(auto it = sources.begin(); it != sources.end(); ++it) {
            uint8_t sourceId = it->first;
            const Source& source = it->second;
            
            RawPacket packet;
            while(source.queue.tryPop(packet)) {
                func(sourceId, packet.data, packet.length);
            }
        }
    }

    bool hasSource(uint8_t sourceId) const {
        return sources.find(sourceId) != sources.end();
    }
    
    size_t getSourceCount() const {
        return sources.size();
    }

private:
    mutable std::map<uint8_t, Source> sources;
    uint8_t nextSourceId = 1;
};

// Global source queue manager - avoid inline (C++17 feature)
extern SourceQueueManager sourceManager;

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
        uint8_t sourceId = sourceManager.createSource(bufferSize, queueSize);
        // Ensure source ID is valid for state tracking
        if (sourceId < sourceStates.size()) {
            resetState(sourceId);
        }
        return sourceId;
    }
    
    // Create a default source if none exists
    void ensureDefaultSource() {
        if (sourceManager.getSourceCount() == 0) {
            DEBUG_PRINT("Creating default source");
            createSource(256, 8);  // Default size
        }
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
        PSRAMVector<uint8_t> headerBuffer;
        PSRAMVector<uint8_t> eventNameBuffer;
        PSRAMVector<uint8_t> eventDataBuffer;
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
    PSRAMVector<EventDispatcherInfo> dispatchers;
    PSRAMVector<RawDataHandler> rawHandlers;
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
    EventMsg() : localAddr(0), groupAddr(0), msgIdCounter(0), unhandledHandler(nullptr) {
        // Initialize all source states
        for(size_t i = 0; i < sourceStates.size(); i++) {
            resetState(i);
        }
    }
    
    ~EventMsg() {
        // Clean up unhandled handler
        if (unhandledHandler != nullptr) {
            delete unhandledHandler;
            unhandledHandler = nullptr;
        }
    }
    
    // Check if PSRAM is enabled
    static bool isPSRAMEnabled() {
#if EVENT_MSG_PSRAM_ENABLED
        return true;
#else
        return false;
#endif
    }

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
