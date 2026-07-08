# Wifi_MIDI

ESP8266 WiFi MIDI bridge source for routing serial MIDI to AppleMIDI/rtpMIDI over WiFi.

## Contents

- `Wifi_MIDI.ino` - Arduino/ESP8266 source.

## Provisioning

The firmware uses WiFiManager for WiFi provisioning. The `MIDI2NET` SSID in the source is the device access-point name used during setup and the advertised AppleMIDI name. Do not hardcode real network passwords in this repository.

## Source Notes

The source references ESP8266 networking libraries, WiFiManager, AppleMIDI, and the Arduino MIDI library.

## License

MIT. See the repository [LICENSE](../LICENSE).
