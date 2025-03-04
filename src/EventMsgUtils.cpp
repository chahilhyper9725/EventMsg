#include "EventMsgUtils.h"

// Concrete handler implementations
namespace {
    template<typename T>
    class ConcreteHandler : public EventHandler {
    public:
        ConcreteHandler(const HandlerConfig& cfg, T cb) 
            : config(cfg), callback(cb) {}

        bool matches(const char* eventName, const uint8_t* header) const override {
            // Check event name if specified
            if (!config.eventName.empty() && 
                config.eventName != eventName) {
                return false;
            }

            // Check sender if filtered
            if (config.hasSenderFilter && 
                config.senderFilter != header[0] && 
                config.senderFilter != 0xFF) {
                return false;
            }

            // Check group if filtered
            if (config.hasGroupFilter && 
                config.groupFilter != header[2] && 
                config.groupFilter != 0x00) {
                return false;
            }

            // Check flags if filtered
            if (config.hasFlagsFilter && 
                config.flagsFilter != header[3]) {
                return false;
            }

            return true;
        }

        void invoke(const char* eventName, const char* data, const uint8_t* header) override {
            invokeCallback(callback, eventName, data, header);
        }

    private:
        HandlerConfig config;
        T callback;

        // Callback invocation specializations
        static void invokeCallback(SimpleCallback& cb, const char*, const char* data, const uint8_t*) {
            cb(data);
        }

        static void invokeCallback(EventCallback& cb, const char* eventName, const char* data, const uint8_t* header) {
            cb(data, header);  // New handler type
        }

        static void invokeCallback(BasicCallback& cb, const char* eventName, const char* data, const uint8_t*) {
            cb(eventName, data);
        }

        static void invokeCallback(DetailedCallback& cb, const char* eventName, const char* data, const uint8_t* header) {
            cb(eventName, data, header[0]); // Pass sender
        }

        static void invokeCallback(FullCallback& cb, const char* eventName, const char* data, const uint8_t* header) {
            cb(eventName, data, header, header[0], header[1], header[3]); // sender, receiver, flags
        }
    };
}

// EventHandlerBuilder implementation
EventHandlerBuilder::EventHandlerBuilder(EventMsgUtils& u, const char* eventName) 
    : utils(u) {
    if (eventName) {
        config.eventName = eventName;
    }
}

EventHandlerBuilder& EventHandlerBuilder::from(uint8_t sender) {
    config.senderFilter = sender;
    config.hasSenderFilter = true;
    return *this;
}

EventHandlerBuilder& EventHandlerBuilder::group(uint8_t groupId) {
    config.groupFilter = groupId;
    config.hasGroupFilter = true;
    return *this;
}

EventHandlerBuilder& EventHandlerBuilder::withFlags(uint8_t flags) {
    config.flagsFilter = flags;
    config.hasFlagsFilter = true;
    return *this;
}

void EventHandlerBuilder::handle(SimpleCallback cb) {
    utils.registerHandler(config, cb);
}

void EventHandlerBuilder::handle(EventCallback cb) {
    utils.registerHandler(config, cb);
}

void EventHandlerBuilder::handle(BasicCallback cb) {
    utils.registerHandler(config, cb);
}

void EventHandlerBuilder::handle(DetailedCallback cb) {
    utils.registerHandler(config, cb);
}

void EventHandlerBuilder::handle(FullCallback cb) {
    utils.registerHandler(config, cb);
}

// RawEventHandlerBuilder implementation
RawEventHandlerBuilder::RawEventHandlerBuilder(EventMsgUtils& u) : utils(u) {}

RawEventHandlerBuilder& RawEventHandlerBuilder::from(uint8_t sender) {
    config.senderFilter = sender;
    config.hasSenderFilter = true;
    return *this;
}

RawEventHandlerBuilder& RawEventHandlerBuilder::group(uint8_t groupId) {
    config.groupFilter = groupId;
    config.hasGroupFilter = true;
    return *this;
}

RawEventHandlerBuilder& RawEventHandlerBuilder::withFlags(uint8_t flags) {
    config.flagsFilter = flags;
    config.hasFlagsFilter = true;
    return *this;
}

void RawEventHandlerBuilder::handle(RawCallback cb) {
    utils.registerRawHandler(config, cb);
}

// EventMsgUtils implementation
EventMsgUtils::EventMsgUtils(EventMsg& msg) : eventMsg(msg) {
    // Register master dispatcher
    eventMsg.registerDispatcher("EventMsgUtils", 0xFF, 0x00,
        [this](const char* eventName, const char* data, const uint8_t* header, uint8_t sender, uint8_t receiver) {
            this->dispatchEvent(eventName, data, header, sender, receiver);
        }
    );
}

EventMsgUtils::~EventMsgUtils() {
    eventMsg.unregisterDispatcher("EventMsgUtils");
    // Clean up handlers
    for (auto* handler : handlers) {
        delete handler;
    }
    handlers.clear();
}

EventHandlerBuilder EventMsgUtils::on(const char* eventName) {
    return EventHandlerBuilder(*this, eventName);
}

RawEventHandlerBuilder EventMsgUtils::onRaw() {
    return RawEventHandlerBuilder(*this);
}

void EventMsgUtils::registerHandler(HandlerConfig config, SimpleCallback cb) {
    handlers.push_back(new ConcreteHandler<SimpleCallback>(config, cb));
}

void EventMsgUtils::registerHandler(HandlerConfig config, EventCallback cb) {
    handlers.push_back(new ConcreteHandler<EventCallback>(config, cb));
}

void EventMsgUtils::registerHandler(HandlerConfig config, BasicCallback cb) {
    handlers.push_back(new ConcreteHandler<BasicCallback>(config, cb));
}

void EventMsgUtils::registerHandler(HandlerConfig config, DetailedCallback cb) {
    handlers.push_back(new ConcreteHandler<DetailedCallback>(config, cb));
}

void EventMsgUtils::registerHandler(HandlerConfig config, FullCallback cb) {
    handlers.push_back(new ConcreteHandler<FullCallback>(config, cb));
}

void EventMsgUtils::registerRawHandler(HandlerConfig config, RawCallback cb) {
    RawHandlerPair pair = {config, cb};
    rawHandlers.push_back(pair);
}

void EventMsgUtils::dispatchEvent(const char* eventName, const char* data, const uint8_t* header, uint8_t sender, uint8_t receiver) {
    // First dispatch to raw handlers
    for (const auto& pair : rawHandlers) {
        const HandlerConfig& config = pair.first;
        const RawCallback& callback = pair.second;
        bool matches = true;

        if (config.hasSenderFilter && 
            config.senderFilter != header[0] && 
            config.senderFilter != 0xFF) {
            matches = false;
        }

        if (matches && config.hasGroupFilter && 
            config.groupFilter != header[2] && 
            config.groupFilter != 0x00) {
            matches = false;
        }

        if (matches && config.hasFlagsFilter && 
            config.flagsFilter != header[3]) {
            matches = false;
        }

        if (matches) {
            // Calculate total message length and get raw data
            size_t nameLen = strlen(eventName);
            size_t dataLen = strlen(data);
            size_t totalLen = 6 + nameLen + 1 + dataLen + 3; // header + name + US + data + control chars
            
            // Call raw handler with complete message
            callback(header - 1, totalLen); // -1 to include SOH
        }
    }

    // Then dispatch to processed handlers
    for (const auto* handler : handlers) {
        if (handler->matches(eventName, header)) {
            handler->invoke(eventName, data, header);
        }
    }
}
