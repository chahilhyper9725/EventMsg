class EventMsg {
    constructor() {
        this.localAddr = 0x00;
        this.groupAddr = 0x00;
        this.msgIdCounter = 0;
        this.writeCallback = null;
        this.dispatchers = [];

        // Control characters
        this.SOH = 0x01;  // Start of Header
        this.STX = 0x02;  // Start of Text
        this.US = 0x1F;   // Unit Separator
        this.EOT = 0x04;  // End of Transmission
        this.ESC = 0x1B;  // Escape Character
    }

    init(callback) {
        this.writeCallback = callback;
        return true;
    }

    setAddr(addr) {
        this.localAddr = addr;
    }

    setGroup(addr) {
        this.groupAddr = addr;
    }

    onEvent(callback, recvId = 0xFF, grpId = 0x00) {
        this.dispatchers.push({
            callback,
            receiverId: recvId,
            groupId: grpId
        });
    }

    byteStuff(data) {
        const result = [];
        const controlChars = [this.SOH, this.STX, this.US, this.EOT, this.ESC];
        
        for (const byte of data) {
            if (controlChars.includes(byte)) {
                result.push(this.ESC, byte ^ 0x20);
            } else {
                result.push(byte);
            }
        }
        return new Uint8Array(result);
    }

    async send(name, data, recvAddr, groupAddr, flags = 0) {
        const msgBuf = [];
        
        // Start message
        msgBuf.push(this.SOH);

        // Header
        const header = this.byteStuff(new Uint8Array([
            this.localAddr,
            recvAddr,
            groupAddr,
            flags,
            (this.msgIdCounter >> 8) & 0xFF,
            this.msgIdCounter & 0xFF
        ]));
        this.msgIdCounter++;

        msgBuf.push(...header);
        msgBuf.push(this.STX);

        // Event name
        const nameBytes = this.byteStuff(new TextEncoder().encode(name));
        msgBuf.push(...nameBytes);
        msgBuf.push(this.US);

        // Event data
        const dataBytes = this.byteStuff(new TextEncoder().encode(data));
        msgBuf.push(...dataBytes);
        msgBuf.push(this.EOT);

        const message = new Uint8Array(msgBuf);
        if (this.writeCallback) {
            return await this.writeCallback(message);
        }
        return false;
    }

    process(data) {
        let state = 'WAITING_FOR_SOH';
        let escapedMode = false;
        let headerBuf = [];
        let nameBuf = [];
        let dataBuf = [];
        let currentBuf = null;

        for (let i = 0; i < data.length; i++) {
            let byte = data[i];

            if (escapedMode) {
                byte ^= 0x20;
                escapedMode = false;
            } else if (byte === this.ESC) {
                escapedMode = true;
                continue;
            }

            switch (state) {
                case 'WAITING_FOR_SOH':
                    if (byte === this.SOH) {
                        state = 'READING_HEADER';
                        currentBuf = headerBuf;
                    }
                    break;

                case 'READING_HEADER':
                    currentBuf.push(byte);
                    if (currentBuf.length === 6) {
                        state = 'WAITING_FOR_STX';
                    }
                    break;

                case 'WAITING_FOR_STX':
                    if (byte === this.STX) {
                        state = 'READING_EVENT_NAME';
                        currentBuf = nameBuf;
                    }
                    break;

                case 'READING_EVENT_NAME':
                    if (byte === this.US) {
                        state = 'READING_EVENT_DATA';
                        currentBuf = dataBuf;
                    } else {
                        currentBuf.push(byte);
                    }
                    break;

                case 'READING_EVENT_DATA':
                    if (byte === this.EOT) {
                        const header = new Uint8Array(headerBuf);
                        const name = new TextDecoder().decode(new Uint8Array(nameBuf));
                        const eventData = new TextDecoder().decode(new Uint8Array(dataBuf));
                        
                        for (const dispatcher of this.dispatchers) {
                            if ((header[1] === dispatcher.receiverId || header[1] === 0xFF) &&
                                (header[2] === dispatcher.groupId || header[2] === 0)) {
                                dispatcher.callback(name, eventData, header, header[0], header[1]);
                            }
                        }

                        // Reset
                        state = 'WAITING_FOR_SOH';
                        headerBuf = [];
                        nameBuf = [];
                        dataBuf = [];
                        currentBuf = null;
                    } else {
                        currentBuf.push(byte);
                    }
                    break;
            }
        }
    }
}

class EventMsgUtils {
    constructor(eventMsg) {
        this.eventMsg = eventMsg;
        this.handlers = [];
        this.rawHandlers = [];

        // Set up master dispatcher
        this.eventMsg.onEvent((name, data, header, sender, receiver) => {
            this.dispatchEvent(name, data, header, sender, receiver);
        }, 0xFF, 0x00);
    }

    on(eventName = null) {
        return new EventHandlerBuilder(this, eventName);
    }

    onRaw() {
        return new RawEventHandlerBuilder(this);
    }

    registerHandler(config, callback) {
        this.handlers.push({config, callback});
    }

    registerRawHandler(config, callback) {
        this.rawHandlers.push({config, callback});
    }

    dispatchEvent(name, data, header, sender, receiver) {
        // First dispatch to raw handlers
        for (const {config, callback} of this.rawHandlers) {
            let matches = true;

            if (config.hasSenderFilter && 
                config.senderFilter !== header[0] && 
                config.senderFilter !== 0xFF) {
                matches = false;
            }

            if (matches && config.hasGroupFilter && 
                config.groupFilter !== header[2] && 
                config.groupFilter !== 0x00) {
                matches = false;
            }

            if (matches && config.hasFlagsFilter && 
                config.flagsFilter !== header[3]) {
                matches = false;
            }

            if (matches) {
                // Calculate message length and get raw data
                const totalLen = header.length + name.length + 1 + data.length + 3;
                callback(header.buffer, totalLen);
            }
        }

        // Then dispatch to processed handlers
        for (const {config, callback} of this.handlers) {
            if (config.eventName && config.eventName !== name) {
                continue;
            }

            if (config.hasSenderFilter && 
                config.senderFilter !== header[0] && 
                config.senderFilter !== 0xFF) {
                continue;
            }

            if (config.hasGroupFilter && 
                config.groupFilter !== header[2] && 
                config.groupFilter !== 0x00) {
                continue;
            }

            if (config.hasFlagsFilter && 
                config.flagsFilter !== header[3]) {
                continue;
            }

            callback(data, header, sender);
        }
    }
}

class HandlerConfig {
    constructor() {
        this.eventName = null;
        this.senderFilter = 0xFF;
        this.groupFilter = 0x00;
        this.flagsFilter = 0x00;
        this.hasSenderFilter = false;
        this.hasGroupFilter = false;
        this.hasFlagsFilter = false;
    }
}

class EventHandlerBuilder {
    constructor(utils, eventName) {
        this.utils = utils;
        this.config = new HandlerConfig();
        if (eventName) {
            this.config.eventName = eventName;
        }
    }

    from(sender) {
        this.config.senderFilter = sender;
        this.config.hasSenderFilter = true;
        return this;
    }

    group(groupId) {
        this.config.groupFilter = groupId;
        this.config.hasGroupFilter = true;
        return this;
    }

    withFlags(flags) {
        this.config.flagsFilter = flags;
        this.config.hasFlagsFilter = true;
        return this;
    }

    handle(callback) {
        this.utils.registerHandler(this.config, callback);
    }
}

class RawEventHandlerBuilder {
    constructor(utils) {
        this.utils = utils;
        this.config = new HandlerConfig();
    }

    from(sender) {
        this.config.senderFilter = sender;
        this.config.hasSenderFilter = true;
        return this;
    }

    group(groupId) {
        this.config.groupFilter = groupId;
        this.config.hasGroupFilter = true;
        return this;
    }

    withFlags(flags) {
        this.config.flagsFilter = flags;
        this.config.hasFlagsFilter = true;
        return this;
    }

    handle(callback) {
        this.utils.registerRawHandler(this.config, callback);
    }
}
