/*
   Copyright (c) 2026 DIODAC ELECTRONICS / iSystem Development
   Licensed under the MIT License.
   Contact: diodac.electronics@gmail.com

   Link_Drum REV 1.01
   Diodac Electronics
   www.diodac.org
*/
#include <MIDI.h>
MIDI_CREATE_DEFAULT_INSTANCE();
#include <avr/pgmspace.h>
#define b_read pgm_read_byte_near
#define w_read pgm_read_dword_near
#define EY         128
#define cv_note    A1
#define cv_pitch   A0
#define cv_bpm     A4
#define cv_changer A2
#define cv_rhythm  A3

#define sq_trigger       2
#define changer          9
#define note_trigger     4
#define velocity_trigger 5
#define play             6
#define midi_mode        7
#define midi_mode_led    8
#define cv_out           3
#define gate_out         10

int rhythm_raw, rhythm;
int pitch_raw, pitch_storage, pitchLSB, pitchMSB, note_raw;
int changer_raw, changer_instrument;

int button_action = HIGH, midi_button_action = HIGH;
boolean on = false, midi_on = false;
byte midi_channel = 9, note, note_MSG;
byte velocity_level = 63, velocity_off = 0;
byte note_hold, note_flag = 0;
byte minNote = 0, maxNote = 127;

//Sequencer
byte note_sq = 0, bLength = 14;
unsigned long time = 0, beats_per_minute = 120;
long last_time = 0;

const PROGMEM byte sequence[34][16] = {
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
  35, 36, 37, 37, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, // 1.  Chromatic
  35, 36, 37, 40, 41, 43, 44, 47, 48, 49, 52, 53, 55, 56, 59, 62, // 2.  Klezmer
  45, 48, 50, 52, 55, 57, 60, 62, 64, 67, 69, 72, 74, 76, 78, 80, // 3.  Major_5
  38, 39, 41, 43, 45, 46, 48, 50, 51, 53, 55, 57, 58, 60, 62, 64, // 4.  Dorian
  41, 43, 44, 47, 48, 50, 51, 53, 55, 56, 59, 60, 62, 63, 66, 68,  // 5.  Aeolian
  48, 51, 53, 55, 57, 60, 63, 65, 67, 69, 72, 75, 77, 79, 81, 82, // 6.  Minor_5
  36, 38, 39, 41, 43, 44, 46, 48, 50, 51, 53, 55, 56, 58, 60, 62, // 7.  Minor
  41, 42, 43, 45, 48, 51, 53, 54, 55, 57, 60, 63, 65, 66, 67, 69  // 8.  Blues
};

void setup() {

  Serial.begin(31250);
  MIDI.begin(MIDI_CHANNEL_OMNI);

  pinMode(velocity_trigger, INPUT);
  pinMode(velocity_trigger, LOW);
  pinMode(note_trigger, INPUT);
  pinMode(note_trigger, LOW);
  pinMode(midi_mode, INPUT);
  pinMode(midi_mode, LOW);
  pinMode(changer, INPUT);
  pinMode(sq_trigger, INPUT_PULLUP);
  pinMode(changer, LOW);
  pinMode(midi_mode_led, OUTPUT);
  pinMode(cv_out, OUTPUT);
  pinMode(play, OUTPUT);
  pinMode(gate_out, OUTPUT);
  pinMode(gate_out, LOW);

  unsigned long milliseconds_per_minute = 10000 * 60;
  time = millis() + milliseconds_per_minute / beats_per_minute;

  MIDI.setHandleNoteOn(HandleNoteOn);
  MIDI.setHandleNoteOff(HandleNoteOff);

}

void loop() {

  midi_button();

  if (midi_on == true) {
    MIDI.read();
    digitalWrite(midi_mode_led, HIGH);

  } else {
    if (midi_on == false) {
      digitalWrite(midi_mode_led, LOW);
      digitalWrite(gate_out, LOW);

      sq_button();
      sequencer();

      // Velocity
      if ((digitalRead(velocity_trigger) == HIGH)) {

        velocity_level = 127;

      } else
      { if ((digitalRead(velocity_trigger) == LOW)) {

          velocity_level = 63;

        }
      }
      beats_per_minute = map(analogRead(cv_bpm), 0.0, 1023.0, 15.0, 415.0);
      rhythm_raw = analogRead(cv_rhythm);
      rhythm = map(rhythm_raw, 0, 1023, 0, 33);

      pitch_raw = analogRead(cv_pitch);
      pitch_storage = map(pitch_raw, 0, 1023, 0, 16383);
      pitchLSB = pitch_storage & 0x007F;
      pitchMSB = (pitch_storage >> 7) & 0x007F;
      for (int i = 0; i < 3; i++) {
        pitch_bend((0xE0 | midi_channel), pitchLSB, pitchMSB);
      }

      changer_raw = analogRead(cv_changer);
      changer_instrument = map(changer_raw, 0, 1023, 1023, 0);
      note_raw = analogRead(cv_note);
      if ((digitalRead(note_trigger) == HIGH && on == false)) {

        note = map(note_raw, 0, 1023, 34, 70);
        note_MSG = note;
      }

      switch (note_flag) {

        case 0:  // wait for tigger

          if ((digitalRead(note_trigger) == HIGH && on == false)) {
            delay(2);
            note_flag = 1;

          }

          break;
        case 1:  //note_on
          digitalWrite(gate_out, HIGH && on == false);
          if (digitalRead(changer) == HIGH && on == false) {
            byte fill = map(changer_instrument, 1023, 0, 0, 30);
            note_MSG =  note_MSG + fill;
          }
          midi_note_on(midi_channel, note_MSG, velocity_level);
          pitch_bend((0xE0 | midi_channel), pitchLSB, pitchMSB);

          note_hold = note_MSG;
          note_flag = 2;

          break;
        case 2:  //note processing
          if ((digitalRead(note_trigger) == HIGH && on == false)) {
            if (note_MSG == note_hold) {
              pitch_bend((0xE0 | midi_channel), pitchLSB, pitchMSB);
            }

            else {
              digitalWrite(gate_out, HIGH);
              midi_note_off(midi_channel, note_hold, velocity_off);
              midi_note_on(midi_channel, note_MSG, velocity_level);
              pitch_bend((0xE0 | midi_channel), pitchLSB, pitchMSB);
              note_hold = note_MSG;

            }
          }

          else {

            note_flag = 3;

          }
          break;
        case 3: //note_off
          digitalWrite(gate_out, LOW);
          midi_note_off(midi_channel, note_hold, velocity_off);
          midi_note_off(midi_channel, note_MSG, velocity_off);
          pitch_bend((0xE0 | midi_channel), pitchLSB, pitchMSB);
          note_flag = 0;
          break;
      }
    }
  }

  delay(30);
}

void sq_button()
{
  int button_state = digitalRead(sq_trigger);

  if (button_state == LOW && button_state != button_action)
  {
    on = ! on;
    button_action = button_state;
    delay(20);
  }

  if (button_state == HIGH && button_state != button_action)
  {
    button_action = button_state;
    delay(20);
  }
}

void midi_button()
{
  int midi_button_state = digitalRead(midi_mode);

  if (midi_button_state == HIGH && midi_button_state != midi_button_action)
  {
    midi_on = ! midi_on;
    midi_button_action = midi_button_state;
    delay(20);
  }

  if (midi_button_state == LOW && midi_button_state != midi_button_action)
  {
    midi_button_action = midi_button_state;
    delay(20);
  }
}

void sequencer() {

  static byte place = 0;
  int count = 0;

  if ((millis() > time) && on == true) {

    midi_note_off(midi_channel, note_sq, velocity_off);
    midi_note_off(midi_channel, note_MSG, velocity_off);

    for (int i = 0; i < 3; i++) {

      digitalWrite(play, HIGH);
      delay(2);
      digitalWrite(play, LOW);
    }

    place++;
    if (place >= bLength) place = 0;
    unsigned long milliseconds_per_minute = 10000 * 60;
    time = millis() + milliseconds_per_minute / beats_per_minute;
    while (b_read(&sequence[rhythm][place]) == 0 && count < bLength) {
      place++;
      if (place >= bLength) place = 0;
      count++;
    }
    pitch_bend((0xE0 | midi_channel), pitchLSB, pitchMSB);
    note_sq = b_read(&sequence[rhythm][place]);
    note_sq = w_read(&sequence[rhythm][place]);

    if (count < bLength && on == true) {
      if (b_read(&sequence[rhythm][place]) != EY) {
        if (digitalRead(changer) == HIGH && on == true) {
          byte fill = map(changer_instrument, 0, 1023, 30, 0);
          note_sq = note_sq + fill;

        } else {
          if ((digitalRead(note_trigger) == HIGH && on == true)) {
            note = map(note_raw, 0, 1023, 30, 0);
            note_sq = note + note_sq;

          }
        }
        midi_note_on(midi_channel, note_sq, velocity_level);
        pitch_bend((0xE0 | midi_channel), pitchLSB, pitchMSB);
      } else {

        if (w_read(&sequence[rhythm][place]) == EY) {
          midi_note_off(midi_channel, note_sq, velocity_off);
        }
      }
    }
    last_time = time;
  }
  if (b_read(&sequence[rhythm][place]) >= minNote && b_read(&sequence[rhythm][place]) <= maxNote ) {

    digitalWrite(gate_out, HIGH);
    analogWrite(cv_out, note_sq);
  } else {
    if (note_sq == EY) {
      digitalWrite(gate_out, LOW);
      analogWrite(cv_out, 0);

    }
  }
  if ( on == false ) {
    digitalWrite(gate_out, LOW);
    midi_note_off(midi_channel, note_sq, velocity_off);
    analogWrite(cv_out, 0);
  }
}


// MIDI Processor

// Send a MIDI note-on message.
void midi_note_on(byte channelMIDI, byte noteMIDI, byte velocityMIDI) {
  midiMsg( (0x90 | channelMIDI), noteMIDI, velocityMIDI);
}

// Send a MIDI note-off message.
void midi_note_off(byte channelMIDI, byte noteMIDI, byte velocityMIDI) {
  midiMsg( (0x80 |  channelMIDI), noteMIDI, velocityMIDI);
}

// Send a MIDI control message.
void midi_modulation(byte channelMIDI, byte modulationCC, byte modulationMIDI) {
  midiMsg( (0xB0 |  channelMIDI), modulationCC, modulationMIDI);

}

// Send a MIDI Pitch_Bend  message.
void pitch_bend(byte channelMIDI, byte LSB, byte MSB) {
  midiMsg( (0xE0 | channelMIDI), LSB, MSB);
}

//  Send a two byte midi message
void midi_program(byte statusProgram, byte data ) {
  Serial.write(byte (statusProgram));
  Serial.write(byte (data));

}

// Send a General MIDI message
void midiMsg(byte cmd, byte data1, byte data2) {
  Serial.write(byte (cmd));
  Serial.write(byte (data1));
  Serial.write(byte (data2));
}

void HandleNoteOn(byte channel, byte incoming_note, byte velocity) {
  if (incoming_note >= minNote && incoming_note <= maxNote ) {
    analogWrite(cv_out, incoming_note);
    digitalWrite(gate_out, HIGH);
  }
}

void HandleNoteOff(byte channel, byte incoming_note, byte velocity) {
  if (incoming_note >= minNote && incoming_note <= maxNote) {
    analogWrite(cv_out, 0);
    digitalWrite(gate_out, LOW);
  }
}
