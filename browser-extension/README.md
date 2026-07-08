# Web DAW Bridge Browser Extension

Chrome Manifest V3 extension that exposes a page-level `window.DawBridge` API and relays MIDI messages between web applications and the local native host.

## Files

- `manifest.json` - extension manifest.
- `background.js` - service worker that manages WebSocket connection, MIDI routing, and offscreen audio control.
- `content.js` - page/content bridge.
- `inject.js` - injected `window.DawBridge` API.
- `popup.html` and `popup.js` - extension UI.
- `offscreen.html` and `offscreen.js` - tab audio capture/normalizer document.

## Development

1. Start the native host from `../native-host`.
2. Open `chrome://extensions/`.
3. Enable Developer mode.
4. Load this folder as an unpacked extension.

For the API contract, see [DEVELOPER_SSOT.md](../DEVELOPER_SSOT.md).

## License

MIT. See the repository [LICENSE](../LICENSE).
