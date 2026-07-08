# diodac_audio

Open source DIODAC / iSystem music technology projects, including browser MIDI tooling, embedded MIDI utilities, DSP pedal firmware, Gerber manufacturing files, and legacy microcontroller source where available.

Music hub: [isystem.app/#music](https://isystem.app/#music)  
Portfolio: [diodac.org](https://diodac.org)  
Contact: [diodac.electronics@gmail.com](mailto:diodac.electronics@gmail.com)

## Project Catalogue

| Project | Contents |
| --- | --- |
| [Web DAW Bridge browser extension](browser-extension/README.md) | Chrome MV3 extension that injects `window.DawBridge` for web apps and relays MIDI/audio controls through the local native host. |
| [Web DAW Bridge native host](native-host/README.md) | Node.js WebSocket service on `ws://localhost:8080` using `easymidi` to route MIDI between the browser extension and local MIDI ports. |
| [Link_Drum](Link_Drum/README.md) | Link_Drum firmware and production assets, including AT89C52 HEX, Atmega 328 Arduino source, Gerbers, BOM/CPL files, DXF panels, and pattern notes. |
| [MIDI_Bass_Guitar](MIDI_Bass_Guitar/README.md) | PIC16F88 bass-to-MIDI assembly source, HEX output, and related source text. |
| [PENELOOPE](PENELOOPE/README.md) | Teensy/Arduino DSP pedal firmware with display/audio processing source plus Gerbers, BOM/CPL, and front-panel files. |
| [Wifi_MIDI](Wifi_MIDI/README.md) | ESP8266 + ESP32-S3 WiFi MIDI bridges (WiFiManager provisioning, AppleMIDI/rtpMIDI). |

The original Web DAW Bridge integration contract remains in [DEVELOPER_SSOT.md](DEVELOPER_SSOT.md). Use it only for the `browser-extension/` and `native-host/` bridge workflow.

## Repository Notes

- Hardware manufacturing files are kept in their original folder layout and filenames to avoid breaking references from CAD/CAM tools.
- Some source paths include spaces, such as `Link_Drum/Link_Drum_Code/Atmega 328/Link_Drum_Rev_1.01/Link_Drum_Rev_1.01.ino`.
- `test.html` is a manual smoke-test page for the Web DAW Bridge API.
- Third-party dependency notes are listed in [NOTICE.md](NOTICE.md).
- Contribution guidelines are in [CONTRIBUTING.md](CONTRIBUTING.md).

## License

This repository is released under the MIT License. See [LICENSE](LICENSE).
