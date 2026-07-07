let socket = null;
let isConnected = false;
let availablePorts = { inputs: [], outputs: [] };
let activeOutPort = null;
let activeInPort = null;
let capturingTabId = null;
let creatingOffscreen = null;

// Connect to Node.js Native Host
function connectWebSocket() {
    socket = new WebSocket('ws://localhost:8080');

    socket.onopen = () => {
        console.log('Connected to Native Host');
        isConnected = true;
    };

    socket.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            if (data.action === 'ports_list') {
                availablePorts = { inputs: data.inputs, outputs: data.outputs };
                // Send to popup if it's open
                chrome.runtime.sendMessage({ action: 'update_ports', ports: availablePorts });
            } 
            else if (data.action === 'midi_in') {
                // Forward MIDI IN data from Native Host to the active tab via content script
                chrome.tabs.query({active: true, currentWindow: true}, function(tabs) {
                    if (tabs[0]) {
                        chrome.tabs.sendMessage(tabs[0].id, {
                            action: 'midi_in',
                            payload: data.payload
                        });
                    }
                });
            }
        } catch (e) {
            console.error('WebSocket message parsing error', e);
        }
    };

    socket.onclose = () => {
        console.log('Disconnected from Native Host. Retrying in 5s...');
        isConnected = false;
        availablePorts = { inputs: [], outputs: [] };
        setTimeout(connectWebSocket, 5000);
    };
}

connectWebSocket();

// Manage Offscreen Document for Audio
async function setupOffscreenDocument(path) {
    if (await chrome.offscreen.hasDocument()) return;
    if (creatingOffscreen) {
        await creatingOffscreen;
    } else {
        creatingOffscreen = chrome.offscreen.createDocument({
            url: path,
            reasons: [chrome.offscreen.Reason.USER_MEDIA],
            justification: 'Capturing tab audio for normalizer and volume control.'
        });
        await creatingOffscreen;
        creatingOffscreen = null;
    }
}

// Handle messages from Popup and Content scripts
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    
    // Status & Routing
    if (request.action === 'get_status') {
        sendResponse({ 
            connected: isConnected, 
            ports: availablePorts,
            activeOut: activeOutPort,
            activeIn: activeInPort,
            isCapturing: capturingTabId !== null
        });
        return true;
    }
    
    if (request.action === 'select_out_port') {
        activeOutPort = request.port;
        if (isConnected) socket.send(JSON.stringify({ action: 'select_out_port', port: request.port }));
    }

    if (request.action === 'select_in_port') {
        activeInPort = request.port;
        if (isConnected) socket.send(JSON.stringify({ action: 'select_in_port', port: request.port }));
    }

    // Outbound MIDI from web app -> DAW
    if (request.action === 'send_midi') {
        if (isConnected && socket.readyState === WebSocket.OPEN) {
            socket.send(JSON.stringify({ action: 'send_midi', payload: request.payload }));
        }
    }

    // Audio Capture commands from Popup
    if (request.action === 'toggle_audio') {
        if (request.enable) {
            chrome.tabs.query({active: true, currentWindow: true}, async (tabs) => {
                const tabId = tabs[0].id;
                capturingTabId = tabId;
                
                // Get stream ID for the active tab
                const streamId = await chrome.tabCapture.getMediaStreamId({ targetTabId: tabId });
                
                await setupOffscreenDocument('offscreen.html');
                
                // Tell offscreen doc to start capturing
                chrome.runtime.sendMessage({
                    action: 'start_audio_capture',
                    streamId: streamId
                });
            });
        } else {
            // Stop capturing
            capturingTabId = null;
            chrome.runtime.sendMessage({ action: 'stop_audio_capture' });
            chrome.offscreen.closeDocument();
        }
    }

    if (request.action === 'set_volume') {
        chrome.runtime.sendMessage({ action: 'set_volume', volume: request.volume });
    }
});
