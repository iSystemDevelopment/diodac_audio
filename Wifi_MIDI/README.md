# Wifi_MIDI

WiFi MIDI bridge: routes hardware serial MIDI ↔ AppleMIDI / rtpMIDI over WiFi.

## Sketches

| Sketch | Target | Notes |
|--------|--------|--------|
| `Wifi_MIDI.ino` | **ESP8266** | DIN MIDI on `Serial` @ 31250 (UART shared — no Serial debug while MIDI is live) |
| `Wifi_ESP32_MIDI/Wifi_ESP32_MIDI.ino` | **ESP32-S3** (pins adjustable) | DIN MIDI on `Serial1`; USB `Serial` free for debug |

Open each sketch folder separately in Arduino IDE / PlatformIO.

## Provisioning

Both builds use **WiFiManager**. On first boot (or after credential wipe), join AP **MIDI2NET** (portal password `!12345678` in source — captive portal only, not a home WiFi password). Do not commit real network passwords.

After join:

- rtpMIDI session name: **MIDI2NET**
- Host: device IP (shown on status page)
- Port: **5004**
- Status page: `http://<device-ip>/`

## ESP32-S3 pins

Defaults in `Wifi_ESP32_MIDI.ino` (override by defining before include or editing the `#define`s):

- `MIDI_RX_PIN` → **18**
- `MIDI_TX_PIN` → **17**
- `STATUS_LED_PIN` → `-1` by default (set to a GPIO to light when rtpMIDI is connected)

## Libraries

- WiFi / WiFiManager (tzapu) — ESP8266 or ESP32 board package as appropriate
- [AppleMIDI](https://github.com/lathoub/Arduino-AppleMIDI-Library) (API style matching `OnConnected` / `begin(name)`)
- [MIDI Library](https://github.com/FortySevenEffects/arduino_midi_library) (FortySevenEffects)

## Maintenance notes (ESP8266 sketch fixes)

- Bridged **all MIDI channels** (`MIDI_CHANNEL_OMNI`; previously defaulted to channel 1 only)
- Status page shows real DIN↔Net message counts and rtpMIDI session state (was random noise)
- AJAX polls `/status` instead of replacing the whole document from `/` incorrectly
- Removed unused / duplicate includes; reconnect reboot if WiFi drops
- Kept Serial at 31250 for DIN — no debug prints on that UART

## License

MIT. See the repository [LICENSE](../LICENSE).
