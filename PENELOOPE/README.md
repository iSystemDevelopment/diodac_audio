# PENELOOPE

PENELOOPE is a DIODAC multi-effect DSP pedal project with Teensy/Arduino source and production assets.

## Contents

- `Peneloope/Peneloope.ino` - main firmware source.
- `Peneloope v2 board/` - Gerber and drill files for the board.
- `PENELOOPE_V2_FP/` - front-panel Gerber and drill files.
- `BOM.csv` and `CPL.csv` - bill of materials and component placement data.
- `FP_V2.dxf` - DXF front-panel file.

## Source Notes

The firmware uses Arduino/Teensy-style libraries visible in the source, including SPI, Wire, ADC, CircularBuffer, Adafruit GFX, Adafruit SSD1306, and Adafruit font headers.

### Firmware maintenance (Rev 2.0)

`Peneloope/Peneloope.ino` was revised for correctness while keeping the original effect set and control layout:

- ADC1 averaging/config no longer applied to ADC0 by mistake
- Modulation wavetable stored as float ±0.99 (was truncated to ~0 as `unsigned int`)
- MCP4921 DAC uses a single 16-bit SPI frame
- Button debounce no longer calls `delayMicroseconds` inside the audio ISR
- Large buffer / looper wipes are deferred to `loop()` so the 60 µs ISR stays short
- Flange and reverb reuse the current input sample instead of over-popping an empty queue
- Clearer pin map, signal-path, and ISR-vs-UI comments

## License

MIT. See the repository [LICENSE](../LICENSE).
