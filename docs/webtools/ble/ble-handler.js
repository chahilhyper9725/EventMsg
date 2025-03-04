class BleHandler {
    constructor() {
        this.device = null;
        this.server = null;
        this.txCharacteristic = null;
        this.rxCharacteristic = null;
        this.eventMsg = new EventMsg();
        this.eventUtils = null;
        this.connected = false;
        this.writeWithResponse = false;
        this.discoveredDevices = new Map();
        this.registeredPeers = new Map();
        
        // Nordic UART Service UUIDs
        this.SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
        this.CHARACTERISTIC_UUID_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
        this.CHARACTERISTIC_UUID_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

        // Event handlers
        this.onDeviceDiscovered = null;
        this.onDeviceStatus = null;
        this.onPeerRegistered = null;
        this.onMessageSent = null;
        this.onError = null;

        // Virtual addresses
        this.BLE_VIRTUAL_ADDR = 0xF0;
        this.BRIDGE_ADDR = 0x01;
    }

    async connect() {
        try {
            this.device = await navigator.bluetooth.requestDevice({
                filters: [{
                    services: [this.SERVICE_UUID]
                }]
            });

            this.server = await this.device.gatt.connect();
            const service = await this.server.getPrimaryService(this.SERVICE_UUID);

            this.rxCharacteristic = await service.getCharacteristic(this.CHARACTERISTIC_UUID_RX);
            this.txCharacteristic = await service.getCharacteristic(this.CHARACTERISTIC_UUID_TX);

            // Initialize EventMsg with write callback
            this.eventMsg.init(async (data) => {
                await this.writeToBLE(data);
                return true;
            });
            this.eventMsg.setAddr(this.BLE_VIRTUAL_ADDR);

            // Initialize EventMsgUtils
            this.eventUtils = new EventMsgUtils(this.eventMsg);

            // Start notifications
            await this.txCharacteristic.startNotifications();
            this.txCharacteristic.addEventListener('characteristicvaluechanged', 
                (event) => this.handleIncomingData(event));

            // Configure event handlers
            this.setupEventHandlers();
            this.setupAdvancedHandlers();

            this.connected = true;
            this.emit('connected');
            return true;

        } catch (error) {
            console.error('BLE Connection error:', error);
            return false;
        }
    }

    setupEventHandlers() {
        // Handle discovery responses
        this.eventUtils.on("DISCOVER_RESPONSE")
            .handle((data, header) => {
                try {
                    const info = JSON.parse(data);
                    this.discoveredDevices.set(info.addr, {
                        ...info,
                        lastSeen: Date.now(),
                        rssi: header ? header[5] : undefined
                    });
                    this.onDeviceDiscovered?.(info);
                } catch (e) {
                    this.emit('error', 'Failed to parse discovery response: ' + e.message);
                }
            });

        // Handle status updates
        this.eventUtils.on("STATUS")
            .handle((data, header, sender) => {
                const device = this.discoveredDevices.get(sender);
                if (device) {
                    device.lastStatus = data;
                    device.lastUpdate = Date.now();
                }
                this.onDeviceStatus?.(sender, data);
            });
    }

    setupAdvancedHandlers() {
        // Handle peer registration confirmation
        this.eventUtils.on("PEER_REGISTERED")
            .handle((data) => {
                try {
                    const info = JSON.parse(data);
                    this.registeredPeers.set(info.addr, {
                        ...info,
                        registeredAt: Date.now()
                    });
                    this.onPeerRegistered?.(info);
                } catch (e) {
                    this.emit('error', 'Failed to parse peer registration: ' + e.message);
                }
            });

        // Handle message delivery status
        this.eventUtils.on("MSG_STATUS")
            .handle((data, header, sender) => {
                try {
                    const status = JSON.parse(data);
                    this.onMessageSent?.(status.msgId, status.success, sender);
                } catch (e) {
                    this.emit('error', 'Failed to parse message status: ' + e.message);
                }
            });

        // Monitor device health
        setInterval(() => {
            const now = Date.now();
            this.discoveredDevices.forEach((device, addr) => {
                if (now - device.lastSeen > 30000) { // 30 seconds timeout
                    this.discoveredDevices.delete(addr);
                    this.emit('deviceTimeout', addr);
                }
            });
        }, 10000);
    }

    emit(event, ...args) {
        const handler = this['on' + event.charAt(0).toUpperCase() + event.slice(1)];
        if (handler) {
            try {
                handler(...args);
            } catch (e) {
                console.error('Handler error:', e);
            }
        }
    }

    async disconnect() {
        if (this.device && this.device.gatt.connected) {
            await this.device.gatt.disconnect();
        }
        this.connected = false;
    }

    setWriteWithResponse(enabled) {
        console.log('Write with response:', enabled);
        this.writeWithResponse = enabled;
    }

    async writeToBLE(data) {
        if (!this.rxCharacteristic) return false;

        let chunkSize = 500;
        let offset = 0;

        while (offset < data.length) {
            let chunk = data.slice(offset, offset + chunkSize);
            offset += chunkSize;
            if (this.writeWithResponse) {
                await this.rxCharacteristic.writeValue(chunk);
            } else {
                await this.rxCharacteristic.writeValueWithoutResponse(chunk);
            }
        }
        return true;
    }

    handleIncomingData(event) {
        const value = new Uint8Array(event.target.value.buffer);
        const hexData = Array.from(value).map(b => b.toString(16).padStart(2, '0')).join(' ');
        console.log('BLE Received:', hexData);
        this.eventMsg.process(value);
    }

    isConnected() {
        return this.connected;
    }

    // API Methods for the Web UI

    startDiscovery() {
        return this.eventMsg.send("DISCOVER", "", 0xFF, 0x00);
    }

    registerPeer(mac, virtualAddr) {
        const data = `MAC:${mac},ADDR:${virtualAddr}`;
        return this.eventMsg.send("REGISTER_PEER", data, this.BRIDGE_ADDR, 0x00);
    }

    forwardToEspNow(data, targetAddr, options = {}) {
        const msgId = Math.floor(Math.random() * 65536);
        const payload = options.json ? JSON.stringify({
            id: msgId,
            data: data
        }) : data;
        
        const flags = (options.priority ? 0x01 : 0x00) |
                     (options.json ? 0x02 : 0x00);
        
        const result = this.eventMsg.send("FORWARD", payload, targetAddr, 0x00, flags);
        
        if (result && options.timeout) {
            setTimeout(() => {
                this.emit('messageTimeout', msgId, targetAddr);
            }, options.timeout);
        }
        
        return result;
    }

    // Event handlers that can be set by the web UI
    onDeviceDiscovered = null;
    onStatusUpdate = null;
}
