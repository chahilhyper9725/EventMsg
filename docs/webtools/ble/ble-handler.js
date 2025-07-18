class BleHandler {
    constructor() {
        this.device = null;
        this.server = null;
        this.txCharacteristic = null;
        this.rxCharacteristic = null;
        this.eventMsg = new EventMsg();
        this.connected = false;
        this.writeWithResponse = false;
        this.pendingWrites = 0;
        this.maxPendingWrites = 3;
        this.adaptiveChunkSize = 500;
        this.writeQueue = [];
        this.throttleDetected = false;
        this.lastWriteTime = 0;
        
        this.SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
        this.CHARACTERISTIC_UUID_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
        this.CHARACTERISTIC_UUID_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
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

            // Initialize EventMsg with optimized write callback
            this.eventMsg.init(async (data) => {
                return await this.writeToBLE(data);
            });

            await this.txCharacteristic.startNotifications();
            this.txCharacteristic.addEventListener('characteristicvaluechanged', 
                (event) => this.handleIncomingData(event));

            this.connected = true;
            
            // Reset optimization parameters on new connection
            this.pendingWrites = 0;
            this.adaptiveChunkSize = 500;
            this.writeQueue = [];
            this.throttleDetected = false;
            
            return true;

        } catch (error) {
            console.error('BLE Connection error:', error);
            return false;
        }
    }

    async disconnect() {
        if (this.device && this.device.gatt.connected) {
            await this.device.gatt.disconnect();
        }
        this.connected = false;
        this.pendingWrites = 0;
        this.writeQueue = [];
    }

    setWriteWithResponse(enabled) {
        console.log('Write with response:', enabled);
        this.writeWithResponse = enabled;
    }

    // Optimized BLE write with flow control and adaptive chunking
    async writeToBLE(data) {
        if (!this.rxCharacteristic) return false;

        // Flow control: limit concurrent writes to prevent browser buffer overflow
        if (this.pendingWrites >= this.maxPendingWrites) {
            // Queue the write if too many pending
            return new Promise((resolve) => {
                this.writeQueue.push({ data, resolve });
            });
        }

        this.pendingWrites++;
        
        try {
            let offset = 0;
            const writePromises = [];
            
            // Adaptive chunking based on throttling detection
            let chunkSize = this.throttleDetected ? 
                Math.min(this.adaptiveChunkSize, 200) : 
                this.adaptiveChunkSize;

            while (offset < data.length) {
                let chunk = data.slice(offset, offset + chunkSize);
                offset += chunkSize;
                
                const writeStart = performance.now();
                
                if (this.writeWithResponse) {
                    await this.rxCharacteristic.writeValue(chunk);
                } else {
                    await this.rxCharacteristic.writeValueWithoutResponse(chunk);
                }
                
                const writeTime = performance.now() - writeStart;
                
                // Less aggressive chunk sizing - only reduce on consistent slow writes
                if (writeTime > 200) {
                    // Only reduce on very slow writes
                    this.adaptiveChunkSize = Math.max(400, this.adaptiveChunkSize - 25);
                    this.throttleDetected = true;
                } else if (writeTime < 5) {
                    // Fast writes, can increase chunk size
                    this.adaptiveChunkSize = Math.min(500, this.adaptiveChunkSize + 10);
                    this.throttleDetected = false; // Reset throttle detection on fast writes
                }
                
                // Only add delay on very slow writes, not general throttling
                if (writeTime > 100 && offset < data.length) {
                    await new Promise(resolve => setTimeout(resolve, 2));
                }
            }
            
            this.pendingWrites--;
            
            // Process queued writes
            if (this.writeQueue.length > 0 && this.pendingWrites < this.maxPendingWrites) {
                const queued = this.writeQueue.shift();
                this.writeToBLE(queued.data).then(queued.resolve);
            }
            
            return true;
            
        } catch (error) {
            this.pendingWrites--;
            console.error('BLE write error:', error);
            
            // Less aggressive error handling - maintain performance
            this.adaptiveChunkSize = Math.max(300, this.adaptiveChunkSize - 50);
            // Don't reduce maxPendingWrites on single errors
            
            return false;
        }
    }

    handleIncomingData(event) {
        const value = new Uint8Array(event.target.value.buffer);
        // Only log hex data when not in high-throughput mode
        if (!this.throttleDetected) {
            const hexData = Array.from(value).map(b => b.toString(16).padStart(2, '0')).join(' ');
            console.log('BLE Received:', hexData);
        }
        this.eventMsg.process(value);
    }

    isConnected() {
        return this.connected;
    }

    onMessageReceived(callback) {
        this.eventMsg.onEvent((name, data, metadata) => {
            callback(name, data, metadata);
        }, 0xFF, 0x00);
    }

    async sendMessage(name, data, receiver, senderGroupId, receiverGroupId, flags) {
        if (!this.connected) return false;
        return this.eventMsg.send(name, data, receiver, senderGroupId, receiverGroupId, flags);
    }

    // Get current BLE performance stats
    getPerformanceStats() {
        return {
            adaptiveChunkSize: this.adaptiveChunkSize,
            pendingWrites: this.pendingWrites,
            queuedWrites: this.writeQueue.length,
            throttleDetected: this.throttleDetected,
            maxPendingWrites: this.maxPendingWrites
        };
    }
}
