# Single Source of Truth (SSOT) - Web DAW Bridge Integration

> Note: this SSOT applies specifically to the Web DAW Bridge in `browser-extension/` and `native-host/`. The rest of this repository also contains hardware and firmware projects with separate README files.

This document serves as the **Single Source of Truth (SSOT)** for AI development teams and human engineers integrating the `Web DAW Bridge` into our web applications.

It defines the standard, professional workflow required to ensure the integration is resilient, maintainable, and easy to upgrade across the entire product ecosystem.

---

## 1. Architectural Standard

The bridge consists of three layers. When building web applications, you are strictly operating at **Layer 3**.

1. **Layer 1: Native Host (`ws://localhost:8080`)** - Manages OS-level API calls (Virtual MIDI cables).
2. **Layer 2: Browser Extension** - Sandboxed relay. Manages user UI (Routing, Volume) and injects the API.
3. **Layer 3: Web Application (Your App)** - Consumes the injected `window.DawBridge` API.

**Golden Rule:** A web application MUST NEVER attempt to connect to Layer 1 (the WebSocket) directly. All communication MUST go through the injected `window.DawBridge` API. This ensures the extension can manage routing, connection states, and permissions securely.

---

## 2. The API Contract

The `window.DawBridge` object is injected globally. You must adhere to the following signatures:

### 2.1 Sending MIDI (Outbound)

Use this function to send data to the DAW.

```typescript
interface DawBridge {
    sendMidi(
        type: 'noteon' | 'noteoff' | 'cc' | 'pitch' | 'poly aftertouch' | 'channel aftertouch' | 'program',
        channel: number,   // 0-15
        note: number,      // 0-127
        velocity: number,  // 0-127
        value?: number     // 0-127 (used for 'cc' or 'pitch')
    ): void;
}
```

### 2.2 Receiving MIDI (Inbound)

Assign a callback function to listen for incoming hardware/DAW data.

```typescript
interface DawBridge {
    onMidiMessage: (message: MidiMessagePayload) => void | null;
}

interface MidiMessagePayload {
    type: string;
    channel: number;
    note?: number;
    velocity?: number;
    value?: number;
    // ...other easymidi fields
}
```

---

## 3. Professional Implementation Workflow

When building or modifying a web app that uses this bridge, the AI team must follow this exact workflow to guarantee stability:

### Step 1: Safe Initialization & Detection

Never assume the extension is installed or active. Your app must check for `window.DawBridge` safely and provide graceful fallbacks.

```javascript
// standard-bridge-init.js
class DAWController {
    constructor() {
        this.isBridgeAvailable = false;
        this.checkBridgeInterval = setInterval(() => this.detectBridge(), 1000);
    }

    detectBridge() {
        if (typeof window !== 'undefined' && window.DawBridge) {
            this.isBridgeAvailable = true;
            clearInterval(this.checkBridgeInterval);
            this.setupListeners();
            console.log("[DAW Integration] Bridge detected and active.");
        }
    }

    setupListeners() {
        window.DawBridge.onMidiMessage = (msg) => this.handleIncoming(msg);
    }

    send(type, channel, note, velocity) {
        if (this.isBridgeAvailable) {
            window.DawBridge.sendMidi(type, channel, note, velocity);
        } else {
            console.warn("[DAW Integration] Attempted to send MIDI, but bridge is not available.");
            // Optional: Fallback to Web Audio API synths here
        }
    }

    handleIncoming(msg) {
        // Handle UI updates or internal logic based on hardware input
    }
}
```

### Step 2: Abstracting the Bridge

Do not scatter `window.DawBridge` calls throughout your React/Vue/Vanilla UI components.
Instead, create a **Singleton Wrapper Class** (like the `DAWController` above) that handles all bridge interactions. Your UI components should only talk to your Wrapper Class. This makes future upgrades to the bridge API trivial, as you only update the Wrapper Class.

### Step 3: Mocking for Tests

When writing automated tests (Jest, Cypress, etc.) or when developing on a machine without the Native Host, the AI team MUST mock the bridge:

```javascript
// Test Mock (setupTests.js)
global.window.DawBridge = {
    sendMidi: jest.fn(),
    onMidiMessage: null
};

// Simulate incoming MIDI during a test
function simulateMidiIn(type, note) {
    if (window.DawBridge.onMidiMessage) {
        window.DawBridge.onMidiMessage({ type, channel: 0, note, velocity: 100 });
    }
}
```

---

## 4. Maintenance and Upgrades

To maintain a professional standard:

1. **Versioning:** If the `window.DawBridge` API signature changes in the future (e.g., adding SysEx support), the Browser Extension MUST bump its version in `manifest.json`.
2. **Backwards Compatibility:** The extension's `inject.js` must maintain support for older function signatures. Never remove `sendMidi`; instead, add new functions (e.g., `sendSysEx`) to ensure older web apps do not break.
3. **Audio Normalization:** The web app developers do NOT need to write code for the audio normalizer. The normalizer operates strictly at Layer 2 (Extension) via `chrome.tabCapture`. Web apps simply output standard HTML5 Audio or Web Audio API, and the extension intercepts it at the browser tab level.
