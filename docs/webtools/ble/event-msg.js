// Control characters
const SOH = 0x01;
const STX = 0x02;
const US = 0x1F;
const EOT = 0x04;
const ESC = 0x1B;

class EventMsg {
    constructor() {
        this.resetState();
        this.localAddr = 0x00;
        this.groupAddr = 0x00;
        this.writeCallback = null;
        this.eventCallback = null;
        this.receiverId = 0x00;
        this.groupId = 0x00;
        this.msgIdCounter = 0;
    }

    resetState() {
        this.state = 'WAITING_FOR_SOH';
        this.headerBuffer = new Uint8Array(6);
        this.eventNameBuffer = new Uint8Array(32);
        this.eventDataBuffer = new Uint8Array(2*1024);
        this.currentBuffer = null;
        this.currentMaxLength = 0;
        this.bufferPos = 0;
        this.escapedMode = false;
        console.log('State machine reset');
    }

    init(writeCallback) {
        this.writeCallback = writeCallback;
        this.resetState();
        return true;
    }

    setAddr(addr) {
        this.localAddr = addr;
    }

    setGroup(addr) {
        this.groupAddr = addr;
    }

    onEvent(callback, recvId, grpId) {
        this.eventCallback = callback;
        this.receiverId = recvId;
        this.groupId = grpId;
    }

    byteStuff(input) {
        const controlChars = [SOH, STX, US, EOT, ESC];
        const output = [];

        for (const byte of input) {
            if (controlChars.includes(byte)) {
                output.push(ESC);
                output.push(byte ^ 0x20);
            } else {
                output.push(byte);
            }
        }

        return new Uint8Array(output);
    }

    send(name, data, recvAddr, groupAddr, flags) {
        // Create header with byte stuffing
        const header = new Uint8Array([
            this.localAddr,
            recvAddr,
            groupAddr,
            flags,
            (this.msgIdCounter >> 8) & 0xFF,
            this.msgIdCounter & 0xFF
        ]);
        this.msgIdCounter++;

        const stuffedHeader = this.byteStuff(header);

        // Convert and stuff event name & data
        const encoder = new TextEncoder();
        const nameBytes = this.byteStuff(encoder.encode(name));
        const dataBytes = this.byteStuff(encoder.encode(data));

        // Build complete message
        const message = new Uint8Array([
            SOH,
            ...stuffedHeader,
            STX,
            ...nameBytes,
            US,
            ...dataBytes,
            EOT
        ]);

        const hexMessage = Array.from(message).map(b => b.toString(16).padStart(2, '0')).join(' ');
        console.log('Sending message:', hexMessage);

        if (this.writeCallback) {
            return this.writeCallback(message);
        }
        return 0;
    }

    processNextByte(byte) {
        // Handle escape sequences
        if (this.escapedMode) {
            byte ^= 0x20;
            this.escapedMode = false;
            //console.log('Escaped byte:', byte.toString(16).padStart(2, '0'));
        } else if (byte === ESC) {
            this.escapedMode = true;
            //console.log('Found escape character');
            return true;
        }

        // Debug state and byte
        const hexByte = byte.toString(16).padStart(2, '0');
        // //console.log(`State: ${this.state}, Processing byte: 0x${hexByte}`);

        // Process based on current state
        switch (this.state) {
            case 'WAITING_FOR_SOH':
                if (byte === SOH) {
                    console.log('Found SOH, switching to READING_HEADER');
                    this.state = 'READING_HEADER';
                    this.currentBuffer = this.headerBuffer;
                    this.currentMaxLength = 6;
                    this.bufferPos = 0;
                }
                break;

            case 'READING_HEADER':
                this.currentBuffer[this.bufferPos++] = byte;
                if (this.bufferPos === this.currentMaxLength) {
                    const headerHex = Array.from(this.headerBuffer).map(b => b.toString(16).padStart(2, '0')).join(' ');
                    console.log('Header complete:', headerHex);
                    this.state = 'WAITING_FOR_STX';
                }
                break;

            case 'WAITING_FOR_STX':
                if (byte === STX) {
                    console.log('Found STX, switching to READING_EVENT_NAME');
                    this.state = 'READING_EVENT_NAME';
                    this.currentBuffer = this.eventNameBuffer;
                    this.currentMaxLength = this.eventNameBuffer.length;
                    this.bufferPos = 0;
                } else {
                    console.error('Invalid sequence: Expected STX');
                    return false;
                }
                break;

            case 'READING_EVENT_NAME':
                if (byte === US) {
                    const nameBytes = this.eventNameBuffer.slice(0, this.bufferPos);
                    const eventName = new TextDecoder().decode(nameBytes);
                    console.log('Found US, event name:', eventName);
                    this.state = 'READING_EVENT_DATA';
                    this.currentBuffer = this.eventDataBuffer;
                    this.currentMaxLength = this.eventDataBuffer.length;
                    this.bufferPos = 0;
                } else {
                    if (this.bufferPos >= this.currentMaxLength) {
                        console.error('Event name too long');
                        return false;
                    }
                    this.currentBuffer[this.bufferPos++] = byte;
                }
                break;

            case 'READING_EVENT_DATA':
                if (byte === EOT) {
                    console.log('Found EOT, message complete');
                    const decoder = new TextDecoder();
                    const receiver = this.headerBuffer[1];
                    const group = this.headerBuffer[2];

                    if ((receiver === this.localAddr || receiver === 0xFF) &&
                        (group === this.groupAddr || group === 0)) {
                        if (this.eventCallback) {
                            const eventName = decoder.decode(this.eventNameBuffer.slice(0, this.bufferPos));
                            const eventData = decoder.decode(this.eventDataBuffer.slice(0, this.bufferPos));
                            console.log('Emitting event:', eventName.trim(), 'Data:', eventData.trim());
                            this.eventCallback(eventName.trim(), eventData.trim());
                        }
                    } else {
                        console.log('Message filtered out - wrong receiver/group');
                    }

                    // Reset for next message
                    this.resetState();
                } else {
                    if (this.bufferPos >= this.currentMaxLength) {
                        console.error('Data too long');
                        return false;
                    }
                    this.currentBuffer[this.bufferPos++] = byte;
                }
                break;
        }

        return true;
    }

    process(data) {
        // console.log('Processing chunk:', Array.from(data).map(b => b.toString(16).padStart(2, '0')).join(' '));
        for (let i = 0; i < data.length; i++) {
            if (!this.processNextByte(data[i])) {
                console.error('Failed to process byte at position', i);
                this.resetState();
                return false;
            }
        }
        return true;
    }
}
