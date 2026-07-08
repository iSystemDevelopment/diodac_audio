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

## License

MIT. See the repository [LICENSE](../LICENSE).
