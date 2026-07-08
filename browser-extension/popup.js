/*
 * Copyright (c) 2026 DIODAC ELECTRONICS / iSystem Development
 * Licensed under the MIT License.
 * Contact: diodac.electronics@gmail.com
 */

document.addEventListener('DOMContentLoaded', () => {
    const statusDiv = document.getElementById('status');
    const outPortSelect = document.getElementById('outPort');
    const inPortSelect = document.getElementById('inPort');
    const audioToggle = document.getElementById('audioToggle');
    const volumeSlider = document.getElementById('volume');
    const volLabel = document.getElementById('volLabel');

    function updateUI() {
        chrome.runtime.sendMessage({ action: 'get_status' }, (response) => {
            if (!response) return;

            // Status
            if (response.connected) {
                statusDiv.textContent = 'Native Host Connected';
                statusDiv.className = 'status connected';
            } else {
                statusDiv.textContent = 'Disconnected';
                statusDiv.className = 'status disconnected';
            }

            // Populate Ports if empty or changed
            if (outPortSelect.options.length <= 1 && response.ports.outputs.length > 0) {
                outPortSelect.innerHTML = '<option value="">None</option>';
                response.ports.outputs.forEach(port => {
                    const opt = document.createElement('option');
                    opt.value = port;
                    opt.textContent = port;
                    if (response.activeOut === port) opt.selected = true;
                    outPortSelect.appendChild(opt);
                });
            }

            if (inPortSelect.options.length <= 1 && response.ports.inputs.length > 0) {
                inPortSelect.innerHTML = '<option value="">None</option>';
                response.ports.inputs.forEach(port => {
                    const opt = document.createElement('option');
                    opt.value = port;
                    opt.textContent = port;
                    if (response.activeIn === port) opt.selected = true;
                    inPortSelect.appendChild(opt);
                });
            }

            // Audio Status
            audioToggle.checked = response.isCapturing;
        });
    }

    // Listeners for UI changes
    outPortSelect.addEventListener('change', (e) => {
        chrome.runtime.sendMessage({ action: 'select_out_port', port: e.target.value });
    });

    inPortSelect.addEventListener('change', (e) => {
        chrome.runtime.sendMessage({ action: 'select_in_port', port: e.target.value });
    });

    audioToggle.addEventListener('change', (e) => {
        chrome.runtime.sendMessage({ action: 'toggle_audio', enable: e.target.checked });
        if(e.target.checked) {
            chrome.permissions.request({permissions: ['tabCapture']}, (granted) => {
                if(!granted) { e.target.checked = false; }
            });
        }
    });

    volumeSlider.addEventListener('input', (e) => {
        const val = e.target.value;
        volLabel.textContent = val;
        // Convert percentage (0-200) to float (0.0 to 2.0)
        chrome.runtime.sendMessage({ action: 'set_volume', volume: val / 100 });
    });

    updateUI();
    
    // Listen for live updates from background
    chrome.runtime.onMessage.addListener((msg) => {
        if (msg.action === 'update_ports') {
            // Force repopulate on next UI update
            outPortSelect.innerHTML = '<option>Select Port...</option>';
            inPortSelect.innerHTML = '<option>Select Port...</option>';
            updateUI();
        }
    });
});
