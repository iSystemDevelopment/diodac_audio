# Web DAW Bridge Native Host

Node.js WebSocket service that exposes local MIDI ports to the Web DAW Bridge browser extension.

## Files

- `server.js` - WebSocket server on `ws://localhost:8080`, MIDI port discovery, routing, and message forwarding.
- `package.json` and `package-lock.json` - Node dependency metadata.

## Run

```bash
npm install
node server.js
```

The host uses `easymidi` for MIDI I/O and `ws` for WebSocket communication. On Windows, a virtual MIDI cable such as loopMIDI may be useful when routing to a DAW.

## Test after reload

| Step | Verify |
|------|--------|
| 1 | `npm install` then `node server.js` — listens on port 8080 |
| 2 | Extension connects; no WebSocket errors in terminal |
| 3 | MIDI IN message from browser reaches selected output port |
| 4 | Hardware MIDI IN forwards to browser client |

Pair with [browser-extension](../browser-extension/README.md) **Test after reload** checklist.

## License

MIT. See the repository [LICENSE](../LICENSE).
