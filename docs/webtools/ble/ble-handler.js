class BleHandler {
    constructor() {
        this.device = null;
        this.server = null;
        this.txCharacteristic = null;
        this.rxCharacteristic = null;
        this.eventMsg = new EventMsg();
        this.connected = false;
        this.writeWithResponse = false;
        
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

            // Initialize EventMsg with write callback
            this.eventMsg.init(async (data) => {
                await this.writeToBLE(data);
                return true;
            });

            await this.txCharacteristic.startNotifications();
            this.txCharacteristic.addEventListener('characteristicvaluechanged', 
                (event) => this.handleIncomingData(event));

            this.connected = true;
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

    onMessageReceived(callback) {
        this.eventMsg.onEvent((name, data, metadata) => {
            callback(name, data, metadata);
        }, 0xFF, 0x00);
    }

    async sendMessage(name, data, receiver, group, flags) {
        if (!this.connected) return false;
        return this.eventMsg.send(name, data, receiver, group, flags);
    }
}
