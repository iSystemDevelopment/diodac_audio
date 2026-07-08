/*
   Copyright (c) 2026 DIODAC ELECTRONICS / iSystem Development
   Licensed under the MIT License.
   Contact: diodac.electronics@gmail.com

   Link_Drum REV 1.01 — ATmega328
   Diodac Electronics · www.diodac.org

   Modes
   -----
   - Sequencer mode (default): internal pattern player + CV controls
   - MIDI mode: MIDI-in -> gate / CV out (handler callbacks)

   Notes about MIDI channel
   ------------------------
   Raw MIDI status bytes use channel index 0..15.
   midi_channel = 9 means General MIDI "Channel 10" (drums).
*/

#include <MIDI.h>
#include <avr/pgmspace.h>

MIDI_CREATE_DEFAULT_INSTANCE();

// PROGMEM helpers (patterns live in flash)
#define b_read  pgm_read_byte_near

// Pattern rest marker (not a valid MIDI note for this device)
#define EY  128

// ---- Analog CV inputs ----
#define cv_pitch    A0
#define cv_note     A1
#define cv_changer  A2
#define cv_rhythm   A3
#define cv_bpm      A4

// ---- Digitals ----
#define sq_trigger        2   // play/stop (INPUT_PULLUP, active LOW)
#define cv_out            3   // PWM CV / note proxy
#define note_trigger      4   // manual note gate / transpose
#define velocity_trigger  5   // velocity select
#define play              6   // click / play pulse out
#define midi_mode         7   // toggle MIDI vs sequencer
#define midi_mode_led     8
#define changer           9   // pattern/instrument fill enable
#define gate_out         10

// How many steps of each 16-deep pattern row are actually played
static const byte kPatternLength = 14;
static const byte kPatternCount  = 34;  // rows in sequence[][]

// MIDI channel index 0..15 (9 == GM drum channel 10)
static const byte kMidiChannel = 9;

static const byte kMinNote = 0;
static const byte kMaxNote = 127;

// ---- Runtime state ----
int rhythm_raw, rhythm;
int pitch_raw, pitch_storage, pitchLSB, pitchMSB, note_raw;
int changer_raw, changer_instrument;
int last_pitch_storage = -1;  // only send pitch bend when it changes

int button_action = HIGH;
int midi_button_action = HIGH;
boolean sequencer_on = false;
boolean midi_mode_on = false;

byte note = 0;
byte note_MSG = 0;
byte note_hold = 0;
byte note_flag = 0;           // 0 idle, 1 onset, 2 sustain, 3 release
byte velocity_level = 63;
static const byte velocity_off = 0;

// Sequencer timing
byte note_sq = 0;
unsigned long next_step_ms = 0;
unsigned long beats_per_minute = 120;

/*
   Pattern table: 34 rhythms/scales x 16 steps (only first kPatternLength used).
   Values are MIDI note numbers, or EY (128) for a rest.
   Last rows are labeled scale helpers in the original source.
*/
const PROGMEM byte sequence[34][16] = {
  // rows 0..25 — performance / drum-style patterns
  36, 39, 41, 43, 45, 48, 51, 53, 55, 57, 60, 48, 51, 53, 55, 57,
  41, 43, 44, 46, 48, 50, 36, 39, 41, 43, 45, 48, 51, 53, 55, 57,
  39, 41, 42, 43, 45, 48, 51, 53, 54, 46, 48, 50, 36, 39, 41, 43,
  36, 37, 41, 43, 44, 48, 49, 53, 55, 56, 60, 46, 48, 50, 36, 39,
  35, 36, 38, 39, 41, 43, 44, 47, 48, 50, 51, 53, 55, 56, 59, 60,
  35, 36, 38, 39, 42, 43, EY, 46, 48, 50, EY, 54, 55, EY, 58, EY,
  39, 41, 43, 44, 46, EY, 49, EY, 53, 55, EY, 58, 60, EY, 63, 65,
  36, 38, EY, 42, EY, 45, 47, 48, 50, EY, EY, 55, 57, EY, 60, EY,
  36, 36, 36, EY, 36, 43, EY, 36, 47, 36, EY, EY, 36, 37, 40, 36,
  36, 39, 36, EY, 52, 50, 36, 50, 47, 36, 41, 36, 41, 37, 40, 36,
  36, 39, 46, 36, 39, 47, 36, 50, 47, 36, 53, 36, 42, 37, 37, 36,
  35, EY, 49, 37, 35, 37, 49, EY, 35, EY, 49, 37, 35, EY, 49, EY,
  39, 39, 99, 36, EY, EY, 36, 36, 36, EY, EY, 49, EY, 38, EY, 47,
  36, EY, 52, EY, 36, EY, 52, EY, 36, EY, 52, EY, 36, EY, 52, EY,
  36, 39, EY, 36, EY, EY, 37, 36, 52, EY, EY, 68, EY, 56, EY, 70,
  EY, 36, 36, 36, EY, EY, 37, 36, 56, EY, EY, 58, EY, 36, EY, 36,
  69, EY, 39, 38, EY, 36, EY, 36, 62, EY, 36, EY, 36, EY, 36, EY,
  79, 75, 79, 68, EY, EY, 47, 36, 82, EY, EY, 38, EY, 56, EY, 50,
  69, 65, 69, 48, EY, EY, 57, 36, 72, EY, EY, 58, EY, 36, EY, 60,
  49, 45, 49, 48, EY, EY, 37, 56, 52, EY, EY, 58, EY, 56, EY, 50,
  39, 58, 45, 39, EY, 39, 36, 36, 36, EY, EY, 61, EY, 71, EY, EY,
  39, 35, 49, 38, EY, EY, 37, 36, 62, EY, EY, 48, EY, 36, EY, 40,
  36, 36, 49, 38, EY, EY, 37, 36, EY, EY, EY, 48, EY, 61, EY, 40,
  36, EY, 36, EY, 38, EY, 36, EY, 36, EY, 36, EY, 38, EY, EY, 43,
  36, EY, 49, 38, EY, EY, 36, 48, 50, 50, EY, 48, EY, 60, EY, 43,
  40, EY, 46, EY, 58, EY, 36, EY, 36, EY, 46, EY, 58, EY, 69, 72,
  // rows 26..33 — scale helpers
  35, 36, 37, 37, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, // Chromatic
  35, 36, 37, 40, 41, 43, 44, 47, 48, 49, 52, 53, 55, 56, 59, 62, // Klezmer
  45, 48, 50, 52, 55, 57, 60, 62, 64, 67, 69, 72, 74, 76, 78, 80, // Major_5
  38, 39, 41, 43, 45, 46, 48, 50, 51, 53, 55, 57, 58, 60, 62, 64, // Dorian
  41, 43, 44, 47, 48, 50, 51, 53, 55, 56, 59, 60, 62, 63, 66, 68, // Aeolian
  48, 51, 53, 55, 57, 60, 63, 65, 67, 69, 72, 75, 77, 79, 81, 82, // Minor_5
  36, 38, 39, 41, 43, 44, 46, 48, 50, 51, 53, 55, 56, 58, 60, 62, // Minor
  41, 42, 43, 45, 48, 51, 53, 54, 55, 57, 60, 63, 65, 66, 67, 69  // Blues
};

// ---- MIDI helpers (raw UART @ 31250) ----
void midiMsg(byte cmd, byte data1, byte data2);
void midi_note_on(byte channel, byte noteMIDI, byte velocityMIDI);
void midi_note_off(byte channel, byte noteMIDI, byte velocityMIDI);
void pitch_bend(byte channel, byte lsb, byte msb);
void midi_all_notes_off(byte channel);

void HandleNoteOn(byte channel, byte incoming_note, byte velocity);
void HandleNoteOff(byte channel, byte incoming_note, byte velocity);

void midi_button();
void sq_button();
void sequencer();
void read_controls();
void update_pitch_bend_if_needed(bool force);
void handle_manual_note();

static byte clamp_midi_note(int value) {
  if (value < 0) return 0;
  if (value > 127) return 127;
  return (byte)value;
}

void setup() {
  // Hardware UART is also the MIDI port on this hardware
  Serial.begin(31250);
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.setHandleNoteOn(HandleNoteOn);
  MIDI.setHandleNoteOff(HandleNoteOff);

  // Buttons / triggers
  pinMode(sq_trigger, INPUT_PULLUP);      // active LOW
  pinMode(velocity_trigger, INPUT);
  digitalWrite(velocity_trigger, LOW);    // pull-up off (external wiring)
  pinMode(note_trigger, INPUT);
  digitalWrite(note_trigger, LOW);
  pinMode(midi_mode, INPUT);
  digitalWrite(midi_mode, LOW);
  pinMode(changer, INPUT);
  digitalWrite(changer, LOW);

  // Outputs
  pinMode(midi_mode_led, OUTPUT);
  pinMode(cv_out, OUTPUT);
  pinMode(play, OUTPUT);
  pinMode(gate_out, OUTPUT);
  digitalWrite(gate_out, LOW);
  digitalWrite(midi_mode_led, LOW);
  digitalWrite(play, LOW);

  // Arm first sequencer step using 60_000 ms/min (was incorrectly 10000*60)
  if (beats_per_minute < 1) beats_per_minute = 1;
  next_step_ms = millis() + (60000UL / beats_per_minute);
}

void loop() {
  midi_button();

  if (midi_mode_on) {
    MIDI.read();
    digitalWrite(midi_mode_led, HIGH);
  } else {
    digitalWrite(midi_mode_led, LOW);

    sq_button();
    read_controls();
    update_pitch_bend_if_needed(false);
    sequencer();
    handle_manual_note();
  }

  // Soft rate-limit for ADC / button scanning (keeps MIDI UX responsive enough)
  delay(15);
}

// -----------------------------------------------------------------------------
// Control reading
// -----------------------------------------------------------------------------

void read_controls() {
  // Velocity switch: high accent vs mid
  if (digitalRead(velocity_trigger) == HIGH) {
    velocity_level = 127;
  } else {
    velocity_level = 63;
  }

  // Tempo (BPM). Keep integer map — Arduino map() is long-based.
  beats_per_minute = (unsigned long)map(analogRead(cv_bpm), 0, 1023, 15, 415);
  if (beats_per_minute < 1) beats_per_minute = 1;

  // Pattern select: 0 .. kPatternCount-1
  rhythm_raw = analogRead(cv_rhythm);
  rhythm = map(rhythm_raw, 0, 1023, 0, kPatternCount - 1);
  if (rhythm < 0) rhythm = 0;
  if (rhythm > (kPatternCount - 1)) rhythm = kPatternCount - 1;

  // 14-bit pitch bend from CV (0..16383)
  pitch_raw = analogRead(cv_pitch);
  pitch_storage = map(pitch_raw, 0, 1023, 0, 16383);
  pitchLSB = pitch_storage & 0x007F;
  pitchMSB = (pitch_storage >> 7) & 0x007F;

  // Instrument / fill amount (inverted pot)
  changer_raw = analogRead(cv_changer);
  changer_instrument = map(changer_raw, 0, 1023, 1023, 0);

  note_raw = analogRead(cv_note);
  // Base note for manual trigger (held while pressed)
  if (digitalRead(note_trigger) == HIGH && !sequencer_on) {
    note = (byte)map(note_raw, 0, 1023, 34, 70);
    note_MSG = note;
  }
}

void update_pitch_bend_if_needed(bool force) {
  if (!force && pitch_storage == last_pitch_storage) {
    return;
  }
  last_pitch_storage = pitch_storage;
  pitch_bend(kMidiChannel, (byte)pitchLSB, (byte)pitchMSB);
}

// -----------------------------------------------------------------------------
// Manual note state machine (sequencer stopped)
// -----------------------------------------------------------------------------

void handle_manual_note() {
  // Only used when the sequencer is not running
  if (sequencer_on) {
    return;
  }

  switch (note_flag) {
    case 0:  // wait for trigger rising edge
      if (digitalRead(note_trigger) == HIGH) {
        delay(2);  // tiny de-noise
        note_flag = 1;
      }
      break;

    case 1:  // note on
      digitalWrite(gate_out, HIGH);

      if (digitalRead(changer) == HIGH) {
        // Add a small fill offset from the changer CV
        int fill = map(changer_instrument, 1023, 0, 0, 30);
        note_MSG = clamp_midi_note((int)note_MSG + fill);
      }

      midi_note_on(kMidiChannel, note_MSG, velocity_level);
      update_pitch_bend_if_needed(true);
      note_hold = note_MSG;
      note_flag = 2;
      break;

    case 2:  // held / legato
      if (digitalRead(note_trigger) == HIGH) {
        if (note_MSG != note_hold) {
          // Note changed while held — legato re-trigger
          digitalWrite(gate_out, HIGH);
          midi_note_off(kMidiChannel, note_hold, velocity_off);
          midi_note_on(kMidiChannel, note_MSG, velocity_level);
          note_hold = note_MSG;
        }
        update_pitch_bend_if_needed(false);
      } else {
        note_flag = 3;
      }
      break;

    case 3:  // note off
      digitalWrite(gate_out, LOW);
      midi_note_off(kMidiChannel, note_hold, velocity_off);
      if (note_MSG != note_hold) {
        midi_note_off(kMidiChannel, note_MSG, velocity_off);
      }
      update_pitch_bend_if_needed(true);
      note_flag = 0;
      break;
  }
}

// -----------------------------------------------------------------------------
// Buttons (edge-detect with simple debounce)
// -----------------------------------------------------------------------------

void sq_button() {
  int button_state = digitalRead(sq_trigger);

  // Active LOW with INPUT_PULLUP
  if (button_state == LOW && button_state != button_action) {
    sequencer_on = !sequencer_on;
    button_action = button_state;
    delay(20);

    if (!sequencer_on) {
      // Stop cleanly: kill sounding sequencer note and drop gate/CV
      midi_note_off(kMidiChannel, note_sq, velocity_off);
      digitalWrite(gate_out, LOW);
      analogWrite(cv_out, 0);
    } else {
      // Start ASAP on next loop tick
      next_step_ms = millis();
    }
  }

  if (button_state == HIGH && button_state != button_action) {
    button_action = button_state;
    delay(20);
  }
}

void midi_button() {
  int midi_button_state = digitalRead(midi_mode);

  // Active HIGH in original hardware wiring
  if (midi_button_state == HIGH && midi_button_state != midi_button_action) {
    midi_mode_on = !midi_mode_on;
    midi_button_action = midi_button_state;
    delay(20);

    // Avoid stuck notes when changing modes
    midi_all_notes_off(kMidiChannel);
    digitalWrite(gate_out, LOW);
    analogWrite(cv_out, 0);
    note_flag = 0;

    if (midi_mode_on) {
      sequencer_on = false;
    }
  }

  if (midi_button_state == LOW && midi_button_state != midi_button_action) {
    midi_button_action = midi_button_state;
    delay(20);
  }
}

// -----------------------------------------------------------------------------
// Sequencer
// -----------------------------------------------------------------------------

void sequencer() {
  static byte place = 0;

  if (!sequencer_on) {
    digitalWrite(gate_out, LOW);
    analogWrite(cv_out, 0);
    return;
  }

  // Time for next step?
  if ((long)(millis() - next_step_ms) >= 0) {
    // End previous step note
    midi_note_off(kMidiChannel, note_sq, velocity_off);

    // Short play/click strobe on the "play" output
    for (int i = 0; i < 3; i++) {
      digitalWrite(play, HIGH);
      delay(2);
      digitalWrite(play, LOW);
    }

    // Advance and schedule next beat (60000 ms / BPM)
    place++;
    if (place >= kPatternLength) place = 0;
    next_step_ms = millis() + (60000UL / beats_per_minute);

    // Skip rests (EY). Old code compared to 0, but rests are EY=128.
    byte skips = 0;
    while (b_read(&sequence[rhythm][place]) == EY && skips < kPatternLength) {
      place++;
      if (place >= kPatternLength) place = 0;
      skips++;
    }

    note_sq = b_read(&sequence[rhythm][place]);
    // BUGFIX: previous firmware then overwrote note_sq with pgm_read_dword_near
    // on a byte table — that corrupted the note value. Use byte reads only.

    update_pitch_bend_if_needed(false);

    if (skips < kPatternLength && note_sq != EY) {
      int sounding = note_sq;

      if (digitalRead(changer) == HIGH) {
        int fill = map(changer_instrument, 0, 1023, 30, 0);
        sounding += fill;
      } else if (digitalRead(note_trigger) == HIGH) {
        // Live transpose from note CV while playing
        int transpose = map(note_raw, 0, 1023, 30, 0);
        sounding += transpose;
      }

      note_sq = clamp_midi_note(sounding);
      midi_note_on(kMidiChannel, note_sq, velocity_level);
      update_pitch_bend_if_needed(true);
    }
  }

  // Gate / CV from current step
  if (note_sq != EY && note_sq >= kMinNote && note_sq <= kMaxNote) {
    digitalWrite(gate_out, HIGH);
    analogWrite(cv_out, note_sq);
  } else {
    digitalWrite(gate_out, LOW);
    analogWrite(cv_out, 0);
  }
}

// -----------------------------------------------------------------------------
// MIDI message builders
// -----------------------------------------------------------------------------

void midi_note_on(byte channel, byte noteMIDI, byte velocityMIDI) {
  // channel is 0..15
  midiMsg((byte)(0x90 | (channel & 0x0F)), noteMIDI, velocityMIDI);
}

void midi_note_off(byte channel, byte noteMIDI, byte velocityMIDI) {
  midiMsg((byte)(0x80 | (channel & 0x0F)), noteMIDI, velocityMIDI);
}

void pitch_bend(byte channel, byte lsb, byte msb) {
  // Pass channel index only — do not pre-OR 0xE0 at the call site
  midiMsg((byte)(0xE0 | (channel & 0x0F)), lsb, msb);
}

void midi_all_notes_off(byte channel) {
  // CC 123 All Notes Off
  midiMsg((byte)(0xB0 | (channel & 0x0F)), 123, 0);
}

void midiMsg(byte cmd, byte data1, byte data2) {
  Serial.write(cmd);
  Serial.write(data1);
  Serial.write(data2);
}

// MIDI-in → gate / CV (when midi_mode_on)
void HandleNoteOn(byte channel, byte incoming_note, byte velocity) {
  (void)channel;
  (void)velocity;
  if (incoming_note >= kMinNote && incoming_note <= kMaxNote) {
    analogWrite(cv_out, incoming_note);
    digitalWrite(gate_out, HIGH);
  }
}

void HandleNoteOff(byte channel, byte incoming_note, byte velocity) {
  (void)channel;
  (void)velocity;
  if (incoming_note >= kMinNote && incoming_note <= kMaxNote) {
    analogWrite(cv_out, 0);
    digitalWrite(gate_out, LOW);
  }
}
