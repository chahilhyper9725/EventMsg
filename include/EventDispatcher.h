#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H

#include "EventMsg.h"
#include <map>
#include <string>

class EventDispatcher {
public:
    // Callback type for individual events with device name
    using EventCallback = std::function<void(const char* deviceName, const char* eventName, const char* data, EventHeader& header)>;
    
    // Constructor sets local address and device name
    EventDispatcher(const char* devName, uint8_t localAddr = 0x00) 
        : deviceName(devName), localAddress(localAddr) {}
    
    // Register event handler
    void on(const char* eventName, EventCallback callback) {
        handlers[eventName] = callback;
    }
    
    // Handle incoming event
    void dispatchEvent(const char* eventName, const char* data, EventHeader& header) {
        auto it = handlers.find(eventName);
        if (it != handlers.end()) {
            it->second(deviceName.c_str(), eventName, data, header);
        }
    }
    
    // Get dispatcher callback for EventMsg registration
    EventDispatcherCallback getHandler() {
        return [this](const char* deviceName, const char* eventName, const char* data, EventHeader& header) {
            this->dispatchEvent(eventName, data, header);
        };
    }
    
    // Create header for sending to a device
    EventHeader createHeader(uint8_t receiverId, uint8_t groupId = 0x00) {
        return EventHeader{
            localAddress,  // our address as sender
            receiverId,
            groupId,
            0x00         // no flags
        };
    }
    
    // Create header for responding to a message
    EventHeader createResponseHeader(const EventHeader& originalHeader) {
        return EventHeader{
            localAddress,          // our address as sender
            originalHeader.senderId, // original sender becomes receiver
            0x00,                  // no group
            0x00                   // no flags
        };
    }
    
    // Get/set local address
    uint8_t getLocalAddress() const { return localAddress; }
    void setLocalAddress(uint8_t addr) { localAddress = addr; }
    
    // Get device name
    const char* getDeviceName() const { return deviceName.c_str(); }

private:
    std::map<std::string, EventCallback> handlers;
    uint8_t localAddress;
    std::string deviceName;
};

#endif // EVENT_DISPATCHER_H
