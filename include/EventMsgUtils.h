#ifndef EVENTMSG_UTILS_H
#define EVENTMSG_UTILS_H

#include "EventMsg.h"
#include <functional>
#include <vector>
#include <string>
#include <utility>

// Forward declarations
class EventHandlerBuilder;
class RawEventHandlerBuilder;
class EventMsgUtils;

// Callback types with varying signatures
using SimpleCallback = std::function<void(const char*)>;
using EventCallback = std::function<void(const char*, const uint8_t*)>;
using BasicCallback = std::function<void(const char*, const char*)>;
using DetailedCallback = std::function<void(const char*, const char*, uint8_t)>;
using FullCallback = std::function<void(const char*, const char*, const uint8_t*, uint8_t, uint8_t, uint8_t)>;
using RawCallback = std::function<void(const uint8_t*, size_t)>;

// Handler configuration
struct HandlerConfig {
    uint8_t senderFilter = 0xFF;
    uint8_t groupFilter = 0x00;
    uint8_t flagsFilter = 0x00;
    bool hasSenderFilter = false;
    bool hasGroupFilter = false;
    bool hasFlagsFilter = false;
    std::string eventName;
};

// Base handler class
class EventHandler {
public:
    virtual ~EventHandler() = default;
    virtual bool matches(const char* eventName, const uint8_t* header) const = 0;
    virtual void invoke(const char* eventName, const char* data, const uint8_t* header) = 0;
};

// Builder for normal event handlers
class EventHandlerBuilder {
public:
    EventHandlerBuilder(EventMsgUtils& utils, const char* eventName = nullptr);
    
    EventHandlerBuilder& from(uint8_t sender);
    EventHandlerBuilder& group(uint8_t groupId);
    EventHandlerBuilder& withFlags(uint8_t flags);
    
    void handle(SimpleCallback cb);
    void handle(EventCallback cb);   // New overload for event+header
    void handle(BasicCallback cb);
    void handle(DetailedCallback cb);
    void handle(FullCallback cb);

private:
    EventMsgUtils& utils;
    HandlerConfig config;
    friend class EventMsgUtils;
};

// Builder for raw event handlers
class RawEventHandlerBuilder {
public:
    RawEventHandlerBuilder(EventMsgUtils& utils);
    
    RawEventHandlerBuilder& from(uint8_t sender);
    RawEventHandlerBuilder& group(uint8_t groupId);
    RawEventHandlerBuilder& withFlags(uint8_t flags);
    
    void handle(RawCallback cb);

private:
    EventMsgUtils& utils;
    HandlerConfig config;
    friend class EventMsgUtils;
};

// Main utils class
class EventMsgUtils {
public:
    EventMsgUtils(EventMsg& eventMsg);
    ~EventMsgUtils();

    // Event handler registration with builder pattern
    EventHandlerBuilder on(const char* eventName = nullptr);
    RawEventHandlerBuilder onRaw();

    // Prevent copying
    EventMsgUtils(const EventMsgUtils&) = delete;
    EventMsgUtils& operator=(const EventMsgUtils&) = delete;

private:
    EventMsg& eventMsg;
    std::vector<EventHandler*> handlers;
    using RawHandlerPair = std::pair<HandlerConfig, RawCallback>;
    std::vector<RawHandlerPair> rawHandlers;

    // Internal registration methods
    void registerHandler(HandlerConfig config, SimpleCallback cb);
    void registerHandler(HandlerConfig config, EventCallback cb);  // New overload
    void registerHandler(HandlerConfig config, BasicCallback cb);
    void registerHandler(HandlerConfig config, DetailedCallback cb);
    void registerHandler(HandlerConfig config, FullCallback cb);
    void registerRawHandler(HandlerConfig config, RawCallback cb);

    // Dispatcher callback
    void dispatchEvent(const char* eventName, const char* data, const uint8_t* header, uint8_t sender, uint8_t receiver);

    friend class EventHandlerBuilder;
    friend class RawEventHandlerBuilder;
};

#endif // EVENTMSG_UTILS_H
