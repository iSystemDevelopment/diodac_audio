// Inject the API script into the main page context
const script = document.createElement('script');
script.src = chrome.runtime.getURL('inject.js');
script.onload = function() {
    this.remove(); // Clean up after execution
};
(document.head || document.documentElement).appendChild(script);

// Listen for messages from the injected script (window.postMessage) going TO the background
window.addEventListener('message', function(event) {
    if (event.source !== window) return;

    if (event.data && event.data.source === 'daw-bridge-api') {
        chrome.runtime.sendMessage({
            action: 'send_midi',
            payload: event.data.payload
        });
    }
});

// Listen for messages from the background going TO the injected script
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (request.action === 'midi_in') {
        window.postMessage({
            source: 'daw-bridge-host',
            payload: request.payload
        }, '*');
    }
});
