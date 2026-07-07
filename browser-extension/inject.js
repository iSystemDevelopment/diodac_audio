// This script is injected into the web page's DOM so it can expose a global API
window.DawBridge = {
    /**
     * Send a MIDI message to the local DAW Bridge Native Host
     * @param {string} type - e.g., 'noteon', 'noteoff', 'cc', 'pitch'
     * @param {number} channel - MIDI channel (0-15)
     * @param {number} note - MIDI note number (0-127)
     * @param {number} velocity - MIDI velocity (0-127)
     * @param {number} value - Used for 'cc' or 'pitch'
     */
    sendMidi: function(type, channel, note, velocity, value) {
        window.postMessage({
            source: 'daw-bridge-api',
            payload: { type, channel, note, velocity, value }
        }, '*');
    },

    /**
     * Set this function to listen for incoming MIDI messages from the selected IN port
     * Example: window.DawBridge.onMidiMessage = (msg) => console.log(msg);
     */
    onMidiMessage: null
};

// Listen for messages from the content script (which came from Native Host)
window.addEventListener('message', function(event) {
    if (event.source !== window) return;
    
    if (event.data && event.data.source === 'daw-bridge-host') {
        if (typeof window.DawBridge.onMidiMessage === 'function') {
            window.DawBridge.onMidiMessage(event.data.payload);
        }
    }
});

console.log('DAW Bridge API injected! Use window.DawBridge.sendMidi() and window.DawBridge.onMidiMessage');
