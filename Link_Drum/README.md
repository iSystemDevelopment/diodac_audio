# Link_Drum

Link_Drum contains firmware and production assets for the DIODAC Link_Drum project.

## Contents

- `Link_Drum_Code/AT89C52/808.hex` - AT89C52 HEX firmware artifact.
- `Link_Drum_Code/Atmega 328/Link_Drum_Rev_1.01/Link_Drum_Rev_1.01.ino` - Atmega 328 Arduino source.
- `Link_Drum_Code/Original Patterns.txt` - original pattern data notes.
- `Link_Drum REV.1.1_PCB/` - Rev 1.1 CPU and panel Gerbers plus BOM/CPL files.
- `Link_Drum_Gerber_Panel/` - panel Gerber files.
- `Link_Drum_Panel/` - DXF panel files.

## Notes

The Atmega source path intentionally keeps the original folder name with a space: `Link_Drum_Code/Atmega 328/Link_Drum_Rev_1.01/Link_Drum_Rev_1.01.ino`.

Hardware and manufacturing filenames are preserved to avoid breaking external references or CAD/CAM workflows.

### Atmega firmware (Rev 1.01) maintenance notes

`Link_Drum_Rev_1.01.ino` was cleaned up for correctness while keeping hardware behaviour:

- BPM step timing uses `60000 / BPM` (was `10000 * 60`)
- Sequencer rest skips look for `EY` (128), not `0`
- PROGMEM pattern reads are byte-only (removed erroneous dword overwrite)
- Pitch-bend messages no longer double-OR the status byte
- `pinMode` / `digitalWrite` setup corrected for button inputs
- Mode / stop paths send note-offs and clear gate-CV to avoid stuck notes

## Test after flash

See [TEST_AFTER_FLASH.md](../TEST_AFTER_FLASH.md) — **Link_Drum** section. Minimum: transport start/stop, BPM timing, rest steps, no stuck MIDI notes.

## License

MIT. See the repository [LICENSE](../LICENSE).
