# Test after flash

Use this checklist after every firmware recompile and upload. Copy the relevant project section into release notes when shipping a binary.

---

## Link_Drum (ATmega 328 / Rev 1.01)

| Step | Verify | Pass |
|------|--------|------|
| 1 | Power-on, no stuck notes on MIDI out | Gate CV idle, no hung notes |
| 2 | Start/stop transport | Pattern stops cleanly; note-offs sent |
| 3 | BPM change | Timing tracks `60000/BPM` ms per step |
| 4 | Rest steps (`EY`) | Silent steps, no false triggers |
| 5 | Pitch bend | No status-byte doubling on MIDI wire |
| 6 | Mode / bypass buttons | Inputs read correctly; bypass clears output |

---

## PENELOOPE (Teensy DSP pedal)

| Step | Verify | Pass |
|------|--------|------|
| 1 | Boot, OLED shows UI | Display init OK |
| 2 | Dry signal path | Audio passes with effects bypassed |
| 3 | Each effect block | Chorus, flange, delay, reverb audible without clicks |
| 4 | Footswitches / pots | Debounce stable; no ISR dropouts |
| 5 | Looper record/play | Buffers clear without audio ISR stalls |
| 6 | Long run (5+ min) | No watchdog or thermal issues |

---

## Wifi_MIDI (ESP8266 or ESP32-S3)

| Step | Verify | Pass |
|------|--------|------|
| 1 | First boot AP **MIDI2NET** | Captive portal reachable |
| 2 | Join home Wi-Fi | Device gets IP; status page loads |
| 3 | rtpMIDI session **MIDI2NET** | DAW/host sees network MIDI port |
| 4 | DIN → Net | Note on/off counted on status page |
| 5 | Net → DIN | Host MIDI reaches hardware out |
| 6 | Wi-Fi drop | Device recovers or reboots per sketch behaviour |
| 7 | ESP8266 only | No Serial debug on MIDI UART @ 31250 |

---

## Web DAW Bridge (extension + native host)

Not a flash workflow — see [browser-extension/README.md](browser-extension/README.md) and [native-host/README.md](native-host/README.md) **Test after reload** sections.

---

**Tip:** Enable serial debug only on UARTs that are not shared with MIDI. Re-test MIDI timing after any debug build.
