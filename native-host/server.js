/*
 * Copyright (c) 2026 DIODAC ELECTRONICS / iSystem Development
 * Licensed under the MIT License.
 * Contact: diodac.electronics@gmail.com
 */

const WebSocket = require('ws');
const easymidi = require('easymidi');

const PORT = 8080;
const wss = new WebSocket.Server({ port: PORT });

let activeMidiOutput = null;
let activeMidiInput = null;

// Send list of ports to all connected clients
function broadcastPorts() {
    const inputs = easymidi.getInputs();
    const outputs = easymidi.getOutputs();
    
    const message = JSON.stringify({
        action: 'ports_list',
        inputs: inputs,
        outputs: outputs
    });

    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(message);
        }
    });
}

// Select an output port
function selectOutPort(portName) {
    if (activeMidiOutput) {
        activeMidiOutput.close();
        activeMidiOutput = null;
    }
    try {
        activeMidiOutput = new easymidi.Output(portName);
        console.log(`MIDI Output connected to: ${portName}`);
    } catch (e) {
        console.error(`Failed to connect to MIDI Output: ${portName}`, e);
    }
}

// Select an input port
function selectInPort(portName) {
    if (activeMidiInput) {
        activeMidiInput.close();
        activeMidiInput = null;
    }
    try {
        activeMidiInput = new easymidi.Input(portName);
        console.log(`MIDI Input connected to: ${portName}`);
        
        // Listen to all standard MIDI messages and forward them
        const messageTypes = ['noteon', 'noteoff', 'cc', 'pitch', 'poly aftertouch', 'channel aftertouch', 'program'];
        messageTypes.forEach(type => {
            activeMidiInput.on(type, (msg) => {
                const wsMsg = JSON.stringify({
                    action: 'midi_in',
                    payload: { type: type, ...msg }
                });
                wss.clients.forEach(client => {
                    if (client.readyState === WebSocket.OPEN) {
                        client.send(wsMsg);
                    }
                });
            });
        });
    } catch (e) {
        console.error(`Failed to connect to MIDI Input: ${portName}`, e);
    }
}

wss.on('connection', function connection(ws) {
    console.log('Browser extension connected!');
    
    // Immediately send available ports
    const inputs = easymidi.getInputs();
    const outputs = easymidi.getOutputs();
    ws.send(JSON.stringify({
        action: 'ports_list',
        inputs: inputs,
        outputs: outputs
    }));

    ws.on('message', function incoming(message) {
        try {
            const data = JSON.parse(message);
            
            // Handle routing commands
            if (data.action === 'select_out_port') {
                selectOutPort(data.port);
            } 
            else if (data.action === 'select_in_port') {
                selectInPort(data.port);
            }
            // Handle outgoing MIDI from browser -> DAW
            else if (data.action === 'send_midi' && activeMidiOutput) {
                const payload = data.payload;
                if (payload && payload.type) {
                    const type = payload.type;
                    delete payload.type;
                    activeMidiOutput.send(type, payload);
                }
            }
        } catch (e) {
            console.error('Error processing message:', e.message);
        }
    });

    ws.on('close', () => {
        console.log('Browser extension disconnected.');
    });
});

console.log(`Native MIDI Host running on ws://localhost:${PORT}`);
console.log('Available Inputs:', easymidi.getInputs());
console.log('Available Outputs:', easymidi.getOutputs());
