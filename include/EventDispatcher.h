#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H

#include "EventMsg.h"
#include <map>
#include <string>

class EventDispatcher {
public:
    // Callback type for individual events with device name and data length
    using EventCallback = std::function<void(const char* data, size_t length, EventHeader& header)>;
    
    // Constructor sets local address, receiver ID, and group ID
    EventDispatcher(uint8_t localAddr = 0x00, uint8_t receiverId = 0xFF, uint8_t groupId = 0x00) 
        : localAddress(localAddr), listenReceiverId(receiverId), listenGroupId(groupId) {}
    
    // Register event handler
    void on(const char* eventName, EventCallback callback) {
        handlers[eventName] = callback;
    }
    
    // Handle incoming event
    void dispatchEvent(const char* eventName, const char* data, size_t length, EventHeader& header) {
        auto it = handlers.find(eventName);
        if (it != handlers.end()) {
            it->second(data, length, header);
        }
    }
    
    // Get dispatcher callback for EventMsg registration
    EventDispatcherCallback getHandler() {
        return [this](const char* deviceName, const char* eventName, const char* data, size_t length, EventHeader& header) {
            this->dispatchEvent(eventName, data, length, header);
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
    
    // Get header for registration with EventMsg
    EventHeader getListenHeader() const {
        return EventHeader{
            BROADCAST_SENDER,  // Accept any sender
            listenReceiverId,
            listenGroupId,
            0x00
        };
    }
    
    // Simplified registration with EventMsg
    bool registerWith(EventMsg& eventMsg, const char* name) {
        return eventMsg.registerDispatcher(name, getListenHeader(), getHandler());
    }
    
    // Get/set local address
    uint8_t getLocalAddress() const { return localAddress; }
    void setLocalAddress(uint8_t addr) { localAddress = addr; }
    
    // Get/set receiver ID
    uint8_t getReceiverId() const { return listenReceiverId; }
    void setReceiverId(uint8_t id) { listenReceiverId = id; }
    
    // Get/set group ID
    uint8_t getGroupId() const { return listenGroupId; }
    void setGroupId(uint8_t id) { listenGroupId = id; }

private:
    std::map<std::string, EventCallback> handlers;
    uint8_t localAddress;
    uint8_t listenReceiverId;
    uint8_t listenGroupId;
};

#endif // EVENT_DISPATCHER_H
