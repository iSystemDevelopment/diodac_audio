# diodac_audio — Diodac Music Open Source

MIT-licensed browser ↔ native MIDI bridge for iSystem web audio apps (InfinityApp, Octopus, iDrum, etc.).

**Hub:** [isystem.app — Diodac Music](https://isystem.app/#music) · **Portfolio:** [diodac.org](https://diodac.org)

**Full developer SSOT:** [DEVELOPER_SSOT.md](DEVELOPER_SSOT.md)

---

## Repository layout

```
browser-extension/   Chrome MV3 — injects window.DawBridge
native-host/         Node.js WebSocket server (port 8080) + MIDI
test.html            Manual API smoke test
```

---

## Quick start

### Native host

```bash
cd native-host
npm install
node server.js
```

Windows: install [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) and create port **`Web DAW Bridge`**.

### Extension

1. Chrome → `chrome://extensions/` → Developer mode
2. **Load unpacked** → select `browser-extension/`
3. Confirm popup shows connected to `ws://localhost:8080`

---

## Universal API (injected)

```javascript
if (window.DawBridge) {
  window.DawBridge.sendMidi('noteon', 0, 60, 100);
  window.DawBridge.onMidiMessage = (msg) => console.log(msg);
}
```

---

## License

MIT — see [LICENSE](LICENSE).

---

## Contact

diodac.electronics@gmail.com · https://isystem.app
