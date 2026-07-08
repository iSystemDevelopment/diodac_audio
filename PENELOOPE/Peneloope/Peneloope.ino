/*
  Copyright (c) 2026 DIODAC ELECTRONICS / iSystem Development
  Licensed under the MIT License.
  Contact: diodac.electronics@gmail.com

  PENELOOPE — multi-effect DSP with looper (Rev 2.0)
  Target: Teensy (IntervalTimer audio ISR @ 60 µs ≈ 16.7 kHz)

  Signal path:
    ADC audioIn -> fx queue -> effect -> queue1 -> wet/dry/looper mix -> MCP4921 DAC
*/

#include <SPI.h>
#include <Wire.h>
#include <ADC.h>
#include <ADC_util.h>
#include <CircularBuffer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

ADC *adc = new ADC();
IntervalTimer timer;                 // audio ISR clock
CircularBuffer<int32_t, 128> queue;  // incoming samples for FX
CircularBuffer<int32_t, 128> queue1; // FX output toward mixer

// ---------- Pins ----------
#define led        4   // FX-parameter LED (lit when ext CV selected)
#define ENC_A      5   // rotary encoder A
#define ENC_B      6   // rotary encoder B
#define button     7   // bypass footswitch (active LOW)
#define dac_latch  9   // MCP4921 LDAC
#define dac_cs    10   // MCP4921 CS
#define recordLed  0
#define replayLed  1
#define recordPin  2   // looper record (active LOW)
#define stopPin    3   // looper stop hold (active LOW)
#define EXT_CV_BTN 22  // external CV engage

// Legacy aliases used throughout FX / UI code
#define A ENC_A
#define B ENC_B

// ---------- OLED ----------
#define OLED_ADDR     0x3C
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Effect names on OLED (index == encoder 0..33)
const char *const nameEffect[34] = {
  "CLEAN", "DISTRO", "FUZZLE", "CRUSH", "OVER", "DETUNE",
  "PITCH-UP", "PITCH-DN", "FLANGE", "PITCHDEL", "DELAY", "DELONG",
  "ECHO", "ECHOLONG", "REVERSE", "TREMOLO", "LFO", "PENTAVER",
  "HEXAVER", "SEPTAVER", "RIDOO", "WOODOO", "MODULATE", "WOBBLE",
  "TRIDULATE", "SINE", "TRIANGLE", "CHORUS", "Q-RES", "DEEP-WAH",
  "WAH-WAH", "ROOM", "REVERPLEX", "REVERB"
};

// Fixed-point multiply for delay-line / flange interpolators
inline uint32_t M32x16(uint32_t x, uint32_t y) {
  return (uint32_t)(((uint32_t)x * (uint32_t)y) >> 16);
}

// ---------- Level meter ----------
#define numGrains 128
int FASTRUN image[numGrains];
int FASTRUN real[numGrains];
volatile int bar = 0;

// ---------- Timing / UI ----------
elapsedMillis counter;       // OLED refresh
elapsedMillis count;         // LFO / tremolo pacing
elapsedMillis button_time;   // looper stop hold
elapsedMillis counterMillis; // looper sample clock
elapsedMillis refresh;       // pot ADC throttle (~4 ms)
bool last_readings = false;
bool state = true;           // bypass switch debounce mirror
bool bypass = false;         // true = hard dry path to DAC
bool ext_state = true;
bool ext_on = false;         // use external CV for FX level
volatile bool cln = false;   // CLEAN / bypass display flag
#define bias 512             // mid-rail for ~10-bit bipolar audio
bool active = false;         // near-silence detector
int32_t output_vol = 0, loop_vol = 0, mix_channel = 0, master = 0;
volatile byte encoder = 0;   // selected effect 0..33
int32_t audioIn = 0, audioOut = 0, main_out = 0, dry = 0;
int32_t input = bias, output = bias, bufferOutput = bias;
int fx_pot = 0, external_cv = 0;
volatile int level = 0;      // FX parameter (pot or external)

// ---------- Shared LPF state ----------
int32_t x_1 = 0, d_1 = 0;
long f_1 = 0;

// ---------- Delay / FX buffers ----------
#define bufferSizeAverage 32
int FASTRUN bufferAverage[bufferSizeAverage];
int indexAverage = 0;
bool wipe = false;           // encoder turned — mute / request flush
volatile uint8_t clear_req = 0; // 1=main delay, 2=AB line, 3=reverb (done in loop)
volatile bool wipe_loop_req = false; // clear looper timestamp table off-ISR
#define bufferSize 48000
int FASTRUN buffer[bufferSize];

// Wavetable length (indices 0..points-1)
#define points 2047
float FASTRUN modulation[points]; // 0.99 * cos — used as ±1 depth by chorus/wah FX
unsigned int FASTRUN sine[points];
unsigned int FASTRUN cosine[points];

// ---------- Distortion helpers ----------
#define ShiftBits_amplitude 2
#define AmplitudeMax 0x0200
int distortion = 0, fuzz = 0, bit_crush = 0, amplitude = 0;
double threshold = 0.2, in_0 = 0;

// ---------- Flange / pitch shifter ----------
#define MIN 2    // ~60 us min delay
#define MAX 400  // ~8.5 ms max delay
#define SIZE 400
#define pitch 11700
byte dir = 1;
unsigned int utime = 0, offset = 0, increment = 0, divider = 0, distance = 0, fractional = 0;
int speed = 0, sample = 0, time = 0; // FX delay/modulation depth parameter
int32_t resultA = 0, resultB = 0, outputA = 0, outputB = 0;

// ---------- Chorus / wah delay lines ----------
#define space 350
#define maximum 300
unsigned int wave = bias;
int modulation_in = 0;
int FASTRUN AB[maximum];
int32_t delay_line = 0, line_A = 0, line_B = 0, calculator = 0;
double fraction = 0; // was int32_t — must be float for interpolators
double down = -0.5, down1 = -0.98, down2 = -0.8;
double up = 0.5, up1 = 0.98, up2 = 0.8;

// ---------- Multi-tap reverb ----------
#define D1 4000
#define D2 4000
#define D3 4000
#define D4 4000
#define D5 10000
#define D6 4000
int DV1 = 100, DV2 = 100, DV3 = 150, DV4 = 200, DV5 = 400, DV6 = 200;
double x1 = 0.69, x2 = 0.64, x3 = 0.59, x4 = 0.54;
double y_1 = 0.99, y_2 = 0.94, y_3 = 0.89, y_4 = 0.84;
int FASTRUN X1[D1], X2[D2], X3[D3], X4[D4], X5[D5], X6[D6];
int32_t S1 = 0, S2 = 0, S3 = 0, S4 = 0, S5 = 0, S6 = 0, S7 = 0;
long DC1 = 0, DC2 = 0, DC3 = 0, DC4 = 0, DC5 = 0, DC6 = 1, DC7 = 1, DC8 = 0;

// ---------- Looper (~8.5 s) ----------
#define loopLength 85000
int32_t loopIn = 0, loopOut = bias;
bool rec = false, looper_stop = false, looper_run = false;
unsigned long startTime = 0, endTime = 0, bufferTime = 0;
bool recording = false, playback = false;
unsigned long place = 0;
volatile int loop_bar = 0;
unsigned long DMAMEM position[loopLength];
unsigned short DMAMEM loopAudio[loopLength];

// Write 12-bit sample to MCP4921 (buffered, gain=1, active: config nibble 0x7)
void DAC(int32_t data) {
  uint16_t word = (uint16_t)((data & 0x0FFF) | 0x7000);
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE3));
  digitalWriteFast(dac_cs, LOW);
  SPI.transfer16(word);  // single 16-bit frame (was double-transferred)
  digitalWriteFast(dac_cs, HIGH);
  delayNanoseconds(15);
  digitalWriteFast(dac_latch, LOW);
  delayNanoseconds(100);
  digitalWriteFast(dac_latch, HIGH);
  delayNanoseconds(15);
  SPI.endTransaction();
}

void setup() {
  pinMode(dac_cs, OUTPUT);
  digitalWriteFast(dac_cs, HIGH);
  pinMode(dac_latch, OUTPUT);
  digitalWriteFast(dac_latch, HIGH);

  // Quiet unused digital buffers on analog pins
  pinMode(14, INPUT_DISABLE);
  pinMode(15, INPUT_DISABLE);
  pinMode(16, INPUT_DISABLE);
  pinMode(17, INPUT_DISABLE);
  pinMode(20, INPUT_DISABLE);
  pinMode(21, INPUT_DISABLE);
  pinMode(23, INPUT_DISABLE);

  pinMode(EXT_CV_BTN, INPUT_PULLUP);
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(button, INPUT_PULLUP);
  pinMode(led, OUTPUT);
  pinMode(recordLed, OUTPUT);
  pinMode(replayLed, OUTPUT);
  digitalWrite(recordLed, LOW);
  digitalWrite(replayLed, LOW);
  pinMode(recordPin, INPUT_PULLUP);
  pinMode(stopPin, INPUT_PULLUP);

  bypass = false;

  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  logo();

  SPI.begin();
  // Audio ISR every 60 µs (~16.7 kHz). Do not pass the period as SPI IRQ id.
  timer.begin(effect, 60);
  timer.priority(128);

  for (unsigned int i = 0; i < bufferSize; i++) {
    buffer[i] = bias;
  }
  buffclr1();
  buffclr2();
  modulation_generator();
  sine_generator();
  cosine_generator();

  // ADC0: 12-bit FX pot (A9)
  adc->adc0->setAveraging(16);
  adc->adc0->setResolution(12);
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::MED_SPEED);
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED);
  adc->adc0->setReference(ADC_REFERENCE::REF_3V3);

  // ADC1: 10-bit audio / CV / mix (was incorrectly setAveraging on adc0)
  adc->adc1->setAveraging(16);
  adc->adc1->setResolution(10);
  adc->adc1->setConversionSpeed(ADC_CONVERSION_SPEED::MED_SPEED);
  adc->adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED);
  adc->adc1->setReference(ADC_REFERENCE::REF_3V3);
}

void loop() {
  // Heavy buffer clears requested by ISR (must not run at audio rate)
  if (clear_req == 1) {
    clear_req = 0;
    buffclr();
  } else if (clear_req == 2) {
    clear_req = 0;
    buffclr1();
  } else if (clear_req == 3) {
    clear_req = 0;
    buffclr2();
  }

  if (wipe_loop_req) {
    wipe_loop_req = false;
    wipeBuffer();
  }

  // OLED refresh — audio stays in IntervalTimer ISR
  if (counter >= 64) {
    counter = 0;
    displayShow();
  }
}

void modulation_generator() {
  // Store actual ±0.99 cosine. Storing into unsigned int truncated nearly all values to 0.
  for (int i = 0; i < points; i++) {
    modulation[i] = (float)(0.99 * cos((2.0 * PI * i) / (double)points));
  }
}

void sine_generator() {
  for (int i = 0; i < points; i++) {
    sine[i] = (unsigned int)(((0.99 + sin((2.0 * PI * i) / (double)points)) * points) / 2.0);
  }
}

void cosine_generator() {
  for (int i = 0; i < points; i++) {
    cosine[i] = (unsigned int)(((0.99 + cos((2.0 * PI * i) / (double)points)) * points) / 2.0);
  }
}

void buffclr() {
  for (unsigned int i = 0; i < bufferSize; i++) {
    buffer[i] = bias;
  }
}

void buffclr1() {
  for (int i = 0; i < maximum; i++) {
    AB[i] = 0;
  }
}

// Zero multi-tap reverb lines (do not drain the live audio queue)
void buffclr2() {
  for (int i = 0; i < D1; i++) X1[i] = 0;
  for (int i = 0; i < D2; i++) X2[i] = 0;
  for (int i = 0; i < D3; i++) X3[i] = 0;
  for (int i = 0; i < D4; i++) X4[i] = 0;
  for (int i = 0; i < D5; i++) X5[i] = 0;
  for (int i = 0; i < D6; i++) X6[i] = 0;
  DC1 = DC2 = DC3 = DC4 = DC5 = DC8 = 0;
  DC6 = DC7 = 1;
  S1 = S2 = S3 = S4 = S5 = S6 = S7 = 0;
}

void effect() {
  // 10-bit audio on A2 (ADC1), mid-rail at `bias`
  audioIn = constrain(adc->adc1->analogRead(A2), 0, 1023);
  active = (audioIn == bias); // near mid-rail = "silent" for synth FX gates
  queue.unshift(audioIn);

  // Level meter (single sample — previous loop wrote the same value 128× per tick)
  if (looper_run == false || bypass == true) {
    int32_t amp = (audioIn - bias) >> 1;
    if (amp < 0) amp = -amp;
    bar = (int)constrain(amp, 0, 127);
  } else {
    bar = map((int)(place / 100UL), 0, 850, 0, 127);
  }

  Button();

  if (bypass == false) { // effects active
    Encoder();
    control();
    save_loop();
    play_loop();
    ext_button();

    if (refresh >= 4) {
      refresh = 0;
      addAverage(adc->adc0->analogRead(A9));
      fx_pot = map(average(), 0, 4095, 0, 1023);
      external_cv = constrain(adc->adc1->analogRead(A7), 0, 1023);
      loop_vol = adc->adc1->analogRead(A0);
      output_vol = adc->adc1->analogRead(A1);
      master = adc->adc1->analogRead(A3);
      mix_channel = adc->adc1->analogRead(A6);

      cln = (encoder == 0);

      if (ext_on) {
        level = external_cv;
        digitalWrite(led, HIGH);
      } else {
        level = fx_pot;
        digitalWrite(led, LOW);
      }

      // Encoder motion: mute output briefly and flush buffers for delay-style FX
      if (!digitalRead(ENC_A) || !digitalRead(ENC_B)) {
        wipe = true;
        bufferOutput = audioOut = main_out = output = input = bias;
      } else {
        wipe = false;
      }

      if (wipe) {
        // Request flush from loop() — avoids multi-ms work inside the audio ISR
        if (encoder >= 9 && encoder <= 14) clear_req = 1;
        else if (encoder == 31 || encoder == 32) clear_req = 1;
        else if (encoder == 20 || encoder == 21 || encoder == 22 ||
                 (encoder >= 26 && encoder <= 30)) clear_req = 2;
        else if (encoder == 33) clear_req = 3;
      }
    }

    switch (encoder) {
      case 0:
        clean_fx();
        break;
      case 1:
        distortion_fx();
        break;
      case 2:
        fuzzle_fx();
        break;
      case 3:
        crush_fx();
        break;
      case 4:
        over_fx();
        break;
      case 5:
        detune_fx();
        break;
      case 6:
        pitch_up_fx();
        break;
      case 7:
        pitch_down_fx();
        break;
      case 8:
        flange_fx();
        break;
      case 9:
        pitchdel_fx();
        break;
      case 10:
        delay_fx();
        break;
      case 11:
        delong_fx();
        break;
      case 12:
        echo_fx();
        break;
      case 13:
        echolong_fx();
        break;
      case 14:
        reverse_fx();
        break;
      case 15:
        tremolo_fx();
        break;
      case 16:
        lfo_fx();
        break;
      case 17:
        pentaver_fx();
        break;
      case 18:
        hexaver_fx();
        break;
      case 19:
        septaver_fx();
        break;
      case 20:
        ridoo_fx();
        break;
      case 21:
        woodoo_fx();
        break;
      case 22:
        modulate_fx();
        break;
      case 23:
        wobble_fx();
        break;
      case 24:
        tridulate_fx();
        break;
      case 25:
        sine_fx();
        break;
      case 26:
        triangle_fx();
        break;
      case 27:
        chorus_fx();
        break;
      case 28:
        qres_fx();
        break;
      case 29:
        deepwah_fx();
        break;
      case 30:
        wahwah_fx();
        break;
      case 31:
        room_fx();
        break;
      case 32:
        reverplex_fx();
        break;
      case 33:
        reverb_fx();
        break;
    }

    // Capture wet sample for looper; pull last FX sample from queue1
    loopIn = constrain(output, 0, 1023);
    while (!queue1.isEmpty()) {
      bufferOutput = queue1.pop();
    }

    // Wet gain, loop gain, dry blend, then master (>>1 maps ~10-bit to 12-bit DAC half-scale)
    bufferOutput = map(bufferOutput - bias, 0, 1023, 0, output_vol) + bias;
    loopOut = map(loopOut - bias, 0, 1023, 0, loop_vol) + bias;
    audioOut = constrain((int32_t)(0.75 * (bufferOutput - bias) + 0.75 * (loopOut - bias) + bias), 0, 1023);
    dry = map(audioIn - bias, 0, 1023, 0, mix_channel) + bias;
    main_out = constrain((int32_t)(0.75 * (audioOut - bias) + 0.75 * (dry - bias) + bias), 0, 1023);
    main_out = map(main_out - bias, 0, 1023, 0, master) + bias;
    DAC(main_out >> 1);
  } else {
    // Hard bypass: dry ADC → DAC
    DAC(audioIn >> 1);
  }
}

/* ---- Effects (called from audio ISR; keep work light) ---- */
void clean_fx() {
  while (!queue.isEmpty()) {
    output = map(queue.pop() - bias, 0, 1023, 0, 1023) + bias;
  }
  if (wipe == false)queue1.unshift(output);
}

void distortion_fx () {
  while (!queue.isEmpty()) {
    x_1 = (queue.pop() - bias);
    f_1 = ((x_1 * 13) / 100) + ((d_1 * 87) / 100);
    d_1 = (int32_t)f_1;
    input = map((int32_t)f_1, 0, 1023, 0, 1023) + bias;
    distortion = map(level, 0, 1023, 590, 560);
    if (input >= distortion)  input += distortion;
    output = constrain(input, 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
  }
  if (wipe == false)queue1.unshift(output);
}

void fuzzle_fx () {
  while (!queue.isEmpty()) {
    x_1 = (queue.pop() - bias);
    f_1 = ((x_1 * 13) / 100) + ((d_1 * 87) / 100);
    d_1 = (int32_t)f_1;
    input = map((int32_t)f_1, 0, 1023, 0, 1023) + bias;
    fuzz = map(level, 0, 1023, 565, 525);
    if (input >= fuzz) input = -32767;
    else if (input <= -fuzz) input = 32768;
    output = constrain(input, 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
  }
  if (wipe == false)queue1.unshift(output);
}

void crush_fx () {
  while (!queue.isEmpty()) {
    x_1 = queue.pop() - bias;
    f_1 = ((x_1 * 13) / 100) + ((d_1 * 87) / 100);
    d_1 = (int32_t)f_1;
    input = (int32_t)f_1 + bias;
    amplitude = abs(input - AmplitudeMax);
    amplitude <<= ShiftBits_amplitude;
    bit_crush = map(level >> 4, 0, 64, 700 , 540);
    if (input >= bit_crush)  input += bit_crush ;
    output = constrain(map(abs(input + (amplitude + bias)), 0, 2047, 0, 1023), 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
  }
  if (wipe == false)queue1.unshift(output);
}

void over_fx() {
  while (!queue.isEmpty()) {
    x_1 = queue.pop() - bias;
    f_1 = ((x_1 * 13) / 100) + ((d_1 * 87) / 100);
    d_1 = (int32_t)f_1;
    input = (int32_t)f_1 + bias;
    in_0 = (double) input / bias - 1;
    if (in_0 <= threshold)
    {
      in_0 = in_0 * (double)(3 + (level >> 8));
    }
    if (in_0 >= threshold && in_0 <= 2 * threshold)
    {
      if (in_0 >= 0)
      {
        in_0 = 2 - 3 * in_0;
        in_0 = in_0 * in_0;
        in_0 = 3 - in_0;
        in_0 = in_0 / 3;
      }
      if (in_0 <= 0)
      {
        in_0 = 2 - 3 * in_0;
        in_0 = in_0 * in_0;
        in_0 = 3 - in_0;
        in_0 = in_0 / 3;
      }
    }
    if (in_0 >= 2 * threshold)
    {
      if (in_0 >= 0) in_0 =  0.99;
      if (in_0 <= 0) in_0 = -0.99;
    }
    output = ((in_0 + 1) * bias) - bias;
    output = constrain(map(output, 0, 1023, 0, 1023) + bias, 0, 1023);
  }
  if (wipe == false)queue1.unshift(output);
}

void detune_fx() {
  while (!queue.isEmpty()) {
    static unsigned int locationIn = SIZE, locationOut = SIZE - fractional;
    buffer[locationIn] = queue.pop() * 5.11;
    locationIn++;
    if (locationIn >= SIZE) locationIn = 0;
    locationOut = locationIn + offset;
    if (locationOut >= SIZE) locationOut -= SIZE;
    outputA = buffer[locationOut];
    locationOut += (SIZE >> 1);
    if (locationOut >= SIZE) locationOut -= SIZE;
    outputB = buffer[locationOut];
    if (offset > (SIZE >> 1)) distance = SIZE - offset;
    else distance = offset;
    resultA = M32x16(outputA, (distance << 7));
    resultB = M32x16(outputB, (((SIZE >> 1) - distance) << 7));
    output = constrain(map(resultA += resultB, 0, 2047, 0, 1023), 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    if (level > bias) {
      fractional += map(level, 511, 1023, 0, 63);
      if (fractional >= 0x0080) {
        offset += (fractional >> 7);
        fractional &= 0x007f;
      }
      if (offset >= SIZE) offset -= SIZE;
    } else if (level < bias) {
      fractional += map(level, 0, 511, 63, 0);
      if (fractional >= 0x0080) {
        offset -= (fractional >> 7);
        fractional &= 0x007f;
      }
      if (offset >= SIZE) offset += SIZE;
    }
  }
  if (wipe == false)queue1.unshift(output);
}

void pitch_up_fx() {
  while (!queue.isEmpty()) {
    static unsigned int locationIn = SIZE, locationOut = SIZE - fractional;
    buffer[locationIn] = queue.pop() * 5.11;
    locationIn++;
    if (locationIn >= SIZE) locationIn = 0;
    locationOut = locationIn + offset;
    if (locationOut >= SIZE) locationOut -= SIZE;
    outputA = buffer[locationOut];
    locationOut += (SIZE >> 1);
    if (locationOut >= SIZE) locationOut -= SIZE;
    outputB = buffer[locationOut];
    if (offset > (SIZE >> 1)) distance = SIZE - offset;
    else distance = offset;
    resultA = M32x16(outputA, (distance << 7));
    resultB = M32x16(outputB, (((SIZE >> 1) - distance) << 7));
    output = constrain(map(resultA += resultB, 0, 2047, 0, 1023), 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    fractional += (level >> 1);
    if (fractional >= 0x0080) {
      offset += (fractional >> 7);
      fractional &= 0x007f;
    }
    if (offset >= SIZE) offset -= SIZE;
  }
  if (wipe == false)queue1.unshift(output);
}

void pitch_down_fx() {
  while (!queue.isEmpty()) {
    static unsigned int locationIn = SIZE, locationOut = SIZE - fractional;
    buffer[locationIn] = queue.pop() * 5.11;
    locationIn++;
    if (locationIn >= SIZE) locationIn = 0;
    locationOut = locationIn + offset;
    if (locationOut >= SIZE) locationOut -= SIZE;
    outputA = buffer[locationOut];
    locationOut += (SIZE >> 1);
    if (locationOut >= SIZE) locationOut -= SIZE;
    outputB = buffer[locationOut];
    if (offset > (SIZE >> 1)) distance = SIZE - offset;
    else distance = offset;
    resultA = M32x16(outputA, (distance << 7));
    resultB = M32x16(outputB, (((SIZE >> 1) - distance) << 7));
    output = constrain(map(resultA += resultB, 0, 2047, 0, 1023), 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    fractional += (level >> 4);
    if (fractional >= 0x0080) {
      offset -= (fractional >> 7);
      fractional &= 0x007f;
    }
    if (offset >= SIZE) offset += SIZE;
  }
  if (wipe == false)queue1.unshift(output);
}

void flange_fx() {
  // One sample per ISR tick — reuse dry (do not pop an empty queue for wet/dry mix)
  while (!queue.isEmpty()) {
    static int locationIn = SIZE, locationOut = SIZE - fractional;
    int32_t dry_s = queue.pop();
    buffer[locationIn] = dry_s * 3.01;
    locationIn++;
    if (locationIn >= SIZE) locationIn = 0;
    locationOut = locationIn - (fractional >> 8);
    if (locationOut < 0) locationOut += SIZE;
    outputA = buffer[locationOut] + dry_s;
    locationOut -= 1;
    if (locationOut < 0) locationOut += SIZE;
    outputB = buffer[locationOut] + dry_s;
    resultA = M32x16(outputA, ((0xff - (fractional & 0x00ff)) << 7));
    resultB = M32x16(outputB, ((fractional & 0x00ff) << 7));
    output = constrain(map(resultA += resultB, 0, 2047, 0, 1023), 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    int shift = level >> 6;
    if (shift >= 11) shift = 11;
    if (dir) {
      if ((fractional >> 8) >= MAX) dir = 0;
      fractional += (1 + shift);
    } else {
      if ((fractional >> 8) <= MIN) dir = 1;
      fractional -= (1 + shift);
    }
  }
  if (wipe == false) queue1.unshift(output);
}

void pitchdel_fx () {
  while (!queue.isEmpty()) {
    time = (level << 2);
    static unsigned int locationIn = pitch, locationOut = pitch - time;
    buffer[locationIn] = queue.pop();
    locationIn++;
    if (locationIn >= pitch) locationIn = 0;
    if (locationOut >= pitch) locationOut = 0;
    if (time <= 2048) { //Downshifting
      if (time <= 1536) {
        if (time <= 1280) {
          if (time <= 1024) {
            if (time <= 768) {
              if (time <= 512) {
                if (time <= 256) { //Pitch octave down
                  divider++;
                  if (divider <= 2) {
                    locationOut = locationOut + 1;
                    divider++;
                  }
                  else divider = 0;
                }
                else { //Pitch fifth down
                  divider++;
                  if (divider <= 4) {
                    locationOut = locationOut + 1;
                    divider++;
                  }
                  else divider = 0;
                }
              }
              else { //Pitch shift two whole tones/major third down: after every 9th sample hold position
                divider++;
                if (divider <= 8) {
                  locationOut = locationOut + 1;
                  divider++;
                }
                else divider = 0;
              }
            }
            else { //Pitch shift one and a half tone/minor third down
              divider++;
              if (divider <= 12) {
                locationOut = locationOut + 1;
                divider++;
              }
              else divider = 0;
            }
          }
          else { //Pitch shift one whole tone down: after every 16th sample hold position
            divider++;
            if (divider <= 16) {
              locationOut = locationOut + 1;
              divider++;
            }
            else divider = 0;
          }
        }
        else { //Pitch shift a half tone down: after every 32nd sample hold position
          divider++;
          if (divider <= 32) {
            locationOut = locationOut + 1;
            divider++;
          }
          else divider = 0;
        }
      }
      else locationOut = locationOut + 1; //No pitchshift
    }
    else { //Upshifting
      if (time >= 2560) {
        if (time >= 3072) {
          if (time >= 3584) { //Two octaves up
            locationOut = locationOut + 4;
          }
          else locationOut = locationOut + 2; //Octave up
        }
        else { //Fifth up
          if (increment == 0) {
            locationOut = locationOut + 2;
            increment++;
          }
          else {
            locationOut = locationOut + 1;
            increment--;
          }
        }
      }
      else locationOut = locationOut + 1; //No pitchshift
    }
    output = map(((buffer[locationOut] - bias) + (audioIn - bias)), 0, 2047, 0, 1023) + bias;
    output = map(output - bias, 0, 1023, 0, 1023) + bias;

  }
  if (wipe == false)queue1.unshift(output);
}

void delay_fx () {
  while (!queue.isEmpty()) {
    utime = map(level << 2, 0, 4095, 2500, 6000);
    static unsigned int bufferIn = bufferSize, bufferOut = bufferSize - utime;
    buffer[bufferIn] = queue.pop();
    bufferIn++;
    if (bufferIn >= utime) bufferIn = 0;
    bufferOut++;
    if (bufferOut >= utime) bufferOut = 0;
    output = map(((buffer[bufferOut] - bias) + (audioIn - bias)), 0, 2047, 0, 1023) + bias;
    output = map(output - bias, 0, 1023, 0, 1023) + bias;

  }
  if (wipe == false)queue1.unshift(output);
}

void delong_fx () {
  while (!queue.isEmpty()) {
    utime = map(level << 2, 0, 4095, 6000, bufferSize);
    static unsigned int bufferIn = bufferSize, bufferOut = bufferSize - utime;
    buffer[bufferIn] = queue.pop();
    bufferIn++;
    if (bufferIn >= utime) bufferIn = 0;
    bufferOut++;
    if (bufferOut >= utime) bufferOut = 0;
    output = map(((buffer[bufferOut] - bias) + (audioIn - bias)), 0, 2047, 0, 1023) + bias;
    output = map(output - bias, 0, 1023, 0, 1023) + bias;

  }
  if (wipe == false)queue1.unshift(output);
}

void echo_fx () {
  while (!queue.isEmpty()) {
    time = map(level << 2, 0, 4095, 1600, 7000);
    static long bufferIn = bufferSize, bufferOut = bufferSize - time;
    buffer[bufferIn] = ((queue.pop() - bias) + ((buffer[bufferIn] - bias) >> 1) + bias);
    bufferIn++;
    if (bufferIn >= time) bufferIn = 0;
    bufferOut++;
    if (bufferOut >= time) bufferOut = 0;
    output = map(((buffer[bufferOut] - bias) + (audioIn - bias)), 0, 2047, 0, 1023) + bias;
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
  }
  if (wipe == false)queue1.unshift(output);

}

void echolong_fx () {
  while (!queue.isEmpty()) {
    time = map(level, 0, 1023, 7000, 28000);
    static long bufferIn = bufferSize, bufferOut = bufferSize - time;
    buffer[bufferIn] = ((queue.pop() - bias) + ((buffer[bufferIn] - bias) >> 1) + bias);
    bufferIn++;
    if (bufferIn >= time) bufferIn = 0;
    bufferOut++;
    if (bufferOut >= time) bufferOut = 0;
    output = map(((buffer[bufferOut] - bias) + (audioIn - bias)), 0, 2047, 0, 1023) + bias;
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
  }
  if (wipe == false)queue1.unshift(output);
}

void reverse_fx() {
  while (!queue.isEmpty()) {
    time = map(level << 2, 0, 4095, 10000, bufferSize);
    static long bufferIn = bufferSize, bufferOut = bufferSize - time;
    buffer[bufferIn] = queue.pop();
    bufferIn++;
    if (bufferIn - 2 >= time) bufferIn = 0;
    bufferOut--;
    if (bufferOut - 2 <= 0 ) bufferOut = time;
    output = constrain(map((buffer[bufferOut] + audioIn), 0, 2047, 0, 1023), 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
  }
  if (wipe == false)queue1.unshift(output);
}

void tremolo_fx () {
  while (!queue.isEmpty()) {
    if (count >= 2) {
      count = 0;
      sample += (1 + (level >> 4));
    }
    if (sample >= 2047)sample = 0;
    x_1 = sine[sample];
    f_1 = ((x_1 * 11) / 100) + ((d_1 * 89) / 100);
    d_1 = (int)f_1;
    output = constrain(map(queue.pop() - bias, 0, 1023, 0, (int)f_1) + bias, 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
  }
  if (wipe == false)queue1.unshift(output);
}

void lfo_fx() {
  while (!queue.isEmpty()) {
    if (count >= 2) {
      count = 0;
      sample += (3 + (level >> 4));
    }
    if (sample >= 1023)sample = 0;
    x_1 = cosine[sample];
    f_1 = ((x_1 * 11) / 100) + ((d_1 * 89) / 100);
    d_1 = (int)f_1;
    output = constrain(map(queue.pop() - bias, 0, 1023, 0, (int)f_1) + bias, 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
  }
  if (wipe == false)queue1.unshift(output);
}

void pentaver_fx() {
  while (!queue.isEmpty()) {
    time = map(level, 0, 1023, 330, 1);
    static long bufferIn = bufferSize, bufferOut = bufferSize - time;
    buffer[bufferIn] = queue.pop();
    bufferIn++;
    if (bufferIn >= space) bufferIn = 0;
    bufferOut = bufferIn + offset;
    if (bufferOut >= space) bufferOut -= space;
    output = buffer[bufferOut];
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    if (dir) {
      if (offset >= (space - (unsigned int)time)) {
        dir = 0;
        offset--;
      }
      else offset++;
    }
    else {
      if (offset <= 4) {
        dir = 1;
        offset--;
      }
      else offset -= 1;
    }
  }
  if (wipe == false)queue1.unshift(output);
}

void hexaver_fx() {
  while (!queue.isEmpty()) {
    time = map(level >> 3, 0, 127, 50, 200);
    static long bufferIn = bufferSize, bufferOut = bufferSize - time;
    buffer[bufferIn] = queue.pop();
    bufferIn++;
    if (bufferIn >= time) bufferIn = 0;
    output = buffer[bufferOut];
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    bufferOut += 2;
    if (bufferOut >= time) bufferOut = 0;
  }
  if (wipe == false)queue1.unshift(output);
}

void septaver_fx() {
  while (!queue.isEmpty()) {
    input = queue.pop();
    time = map(level >> 6, 0, 31, 15, 100);
    speed++;
    if (speed >= time) {
      speed = 0;
      output = map(input - bias, 0, 1023, 0, 1023) + bias;
    }
    if (wipe == false)queue1.unshift(output);
  }
}

void ridoo_fx() {
  while (!queue.isEmpty()) {
    if (count >= 1) {
      count = 0;
      sample += (40 + (level >> 3));
    }
    if (sample >= 1023)sample = 0;
    modulation_in = constrain(map(cosine[sample], 0, 2047, 0, 63), 0, 63);
    delay_line = modulation_in / 2;
    for (int index = 0; index <= modulation_in; index++)
      AB[modulation_in + 1 - index] = AB[modulation_in - index];
    AB[0] = (queue.pop() - bias);
    line_A = delay_line - delay_line * modulation[calculator * 2];
    line_B =  int32_t(line_A);
    fraction = line_A - line_B;
    if (fraction == 0) fraction = 0.01;
    if (fraction == 1) fraction = 0.99;
    calculator++;
    if (calculator * 2 >= points) calculator = 0;
    output = constrain(map((int32_t)(AB[line_B + 1] * fraction + AB[line_B] * (1 - fraction)), 0, 1023, 0, 1023) + bias, 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    if (output <= 0 ) output = 0.01;
  }
  if (wipe == false)queue1.unshift(output);
}

void woodoo_fx() {
  while (!queue.isEmpty()) {
    if (count >= 1) {
      count = 0;
      sample += (20 + (level >> 3));
    }
    if (sample >= 2047)sample = 0;
    modulation_in = constrain(map(sine[sample], 0, 2047, 0, 63), 0, 63);
    delay_line = modulation_in / 2;
    for (int index = 0; index <= modulation_in; index++)
      AB[modulation_in + 1 - index] = AB[modulation_in - index];
    AB[0] = (queue.pop() - bias);
    line_A = delay_line - delay_line * modulation[calculator * 2];
    line_B =  int32_t(line_A);
    fraction = line_A - line_B;
    if (fraction == 0) fraction = 0.01;
    if (fraction == 1) fraction = 0.99;
    calculator++;
    if (calculator * 2 >= points) calculator = 0;
    output = constrain(map((int32_t)(AB[line_B + 1] * fraction + AB[line_B] * (1 - fraction)), 0, 1023, 0, 1023) + bias, 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    if (output <= 0 ) output = 0.01;
  }
  if (wipe == false)queue1.unshift(output);
}

void modulate_fx() {
  while (!queue.isEmpty()) {
    if (count >= 2) {
      count = 0;
      sample += (300 + (level >> 2));
    }
    if (sample >= 2047)sample = 0;
    modulation_in = constrain(map(cosine[sample], 0, 2047, 0, 63), 0, 63);
    delay_line = modulation_in;
    for (int index = 0; index <= modulation_in; index++)
      AB[modulation_in + 1 - index] = AB[modulation_in - index];
    AB[0] = up2 * (queue.pop() - bias) + down2 * AB[delay_line] ;
    line_A = delay_line - delay_line * modulation[calculator * 2];
    line_B =  int32_t(line_A);
    fraction = line_A - line_B;
    if (fraction == 0) fraction = 0.01;
    if (fraction == 1) fraction = 0.99;
    calculator++;
    if (calculator * 2 >= points) calculator = 0;
    output = constrain(map((int32_t)(AB[line_B + 1] * fraction + AB[line_B] * (1 - fraction)), 0, 1023, 0, 1023) + bias, 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    if (output <= 0 ) output = 0.01;
  }
  if (wipe == false)queue1.unshift(output);
}

void wobble_fx() {
  while (!queue.isEmpty()) {
    x_1 = (queue.pop() - bias) << 2;
    f_1 = ((x_1 * 13) / 100) + ((d_1 * 87) / 100);
    d_1 = (int32_t)f_1;
    input = map((int32_t)f_1, 0, 4095, 0, 1023) + bias;
    sample += map(level >> 4, 0, 63, 8, 63);
    if (sample >= 2047)sample = 0;
    if (input > 540 || input < 484) {
      wave = map(sine[sample], 0, 2047, 0, 1023);
      output = constrain(map((input += wave) * 1.6, 0, 2047, 0, 1023), 0, 1023);
    }
    if (active && playback) output = bias;
  }
  output = map(output - bias, 0, 1023, 0, 1023) + bias;
  if (wipe == false)queue1.unshift(output);
}

void tridulate_fx() {
  while (!queue.isEmpty()) {
    x_1 = (queue.pop() - bias) << 2;
    f_1 = ((x_1 * 13) / 100) + ((d_1 * 87) / 100);
    d_1 = (int32_t)f_1;
    input = map((int32_t)f_1, 0, 4095, 0, 1023) + bias;
    sample += map(level >> 4, 0, 63, 8, 63);
    if (sample >= 1023)sample = 0;
    if (input > 540 || input < 484) {
      wave = map(cosine[sample], 0, 2047, 0, 1023);
      output = constrain(map((input += wave) * 1.6, 0, 2047, 0, 1023), 0, 1023);
    }
    if (active && playback) output = bias;
  }
  output = map(output - bias, 0, 1023, 0, 1023) + bias;
  if (wipe == false)queue1.unshift(output);
}

void sine_fx() {
  while (!queue.isEmpty()) {
    sample += map(level >> 3, 0, 127, 4, 127);
    if (sample >= 2047)sample = 0;
    if  (queue.pop() > 535) {
      output = constrain(map(sine[sample], 0, 2047, 0, 1023), 0, 1023);
    }
    if (active && playback) output = bias;
  }
  output = map(output - bias, 0, 1023, 0, 1023) + bias;
  if (wipe == false)queue1.unshift(output);
}

void triangle_fx() {
  while (!queue.isEmpty()) {
    sample += map(level >> 4, 0, 64, 2, 64);
    if (sample >= 1023)sample = 0;
    if (queue.pop() > 535) {
      output = constrain(map(cosine[sample], 0, 2047, 0, 1023), 0, 1023);
    }
    if (active && playback) output = bias;
  }
  output = map(output - bias, 0, 1023, 0, 1023) + bias;
  if (wipe == false)queue1.unshift(output);
}

void chorus_fx() {
  while (!queue.isEmpty()) {
    modulation_in = (5 + (level >> 3));
    delay_line = modulation_in / 2;
    for (int index = 0; index <= modulation_in; index++)
      AB[modulation_in + 1 - index] = AB[modulation_in - index];
    AB[0] = up * (queue.pop() - bias) * 1.20  + down * AB[delay_line];
    line_A = delay_line - delay_line * modulation[calculator * 2];
    line_B =  int32_t(line_A);
    fraction = line_A - line_B;
    if (fraction == 0) fraction = 0.01;
    if (fraction == 1) fraction = 0.99;
    calculator++;
    if (calculator * 2 >= points) calculator = 0;
    output = constrain(map((int32_t)(AB[line_B + 1] * fraction + AB[line_B] * (1 - fraction)), 0, 1023, 0, 1023) + bias, 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    if (output <= 0 ) output = 0.01;
  }
  if (wipe == false)queue1.unshift(output);
}

void qres_fx() {
  while (!queue.isEmpty()) {
    modulation_in = constrain(map(level, 0, 1023, 10, 295), 10, 295);
    delay_line = modulation_in / 2;
    for (int index = 0; index <= modulation_in; index++)
      AB[modulation_in + 1 - index] = AB[modulation_in - index];
    AB[0] = up1 * (queue.pop() - bias) + down1 * AB[delay_line];
    line_A = delay_line - delay_line * modulation[calculator * 2];
    line_B =  int32_t(line_A);
    fraction = line_A - line_B;
    if (fraction == 0) fraction = 0.01;
    if (fraction == 1) fraction = 0.99;
    calculator++;
    if (calculator * 2 >= points) calculator = 0;
    output = constrain(map((int32_t)(AB[line_B + 1] * fraction + AB[line_B] * (1 - fraction)), 0, 1023, 0, 1023) + bias, 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    if (output <= 0 ) output = 0.01;
  }
  if (wipe == false)queue1.unshift(output);
}

void deepwah_fx() {
  while (!queue.isEmpty()) {
    if (count >= 5) {
      count = 0;
      sample += (1 + (level >> 5));
    }
    if (sample >= 2047)sample = 0;
    modulation_in = constrain(map(cosine[sample], 0, 2047, 120, 220), 80, 280);
    delay_line = modulation_in / 2;
    for (int index = 0; index <= modulation_in; index++)
      AB[modulation_in + 1 - index] = AB[modulation_in - index];
    AB[0] = (up1 * (queue.pop() - bias) + down1 * AB[delay_line]);
    line_A = delay_line - delay_line * modulation[calculator * 2];
    line_B =  int32_t(line_A);
    fraction = line_A - line_B;
    if (fraction == 0) fraction = 0.01;
    if (fraction == 1) fraction = 0.99;
    calculator++;
    if (calculator * 2 >= points) calculator = 0;
    output = constrain(map((int32_t)(AB[line_B + 1] * fraction + AB[line_B] * (1 - fraction)), 0, 2047, 0, 1023) + bias, 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    if (output <= 0 ) output = 0.01;
  }
  if (wipe == false)queue1.unshift(output);
}

void wahwah_fx() {
  while (!queue.isEmpty()) {
    if (count >= 5) {
      count = 0;
      sample += (1 + (level >> 3));
    }
    if (sample >= 2047)sample = 0;
    modulation_in = constrain(map(cosine[sample], 0, 2047, 16, 48), 16, 48);
    delay_line = modulation_in / 2;
    for (int index = 0; index <= modulation_in; index++)
      AB[modulation_in + 1 - index] = AB[modulation_in - index];
    AB[0] = up2 * (queue.pop() - bias) + down2 * AB[delay_line];
    line_A = delay_line - delay_line * modulation[calculator * 2];
    line_B =  int32_t(line_A);
    fraction = line_A - line_B;
    if (fraction == 0) fraction = 0.01;
    if (fraction == 1) fraction = 0.99;
    calculator++;
    if (calculator * 2 >= points) calculator = 0;
    output = constrain(map((int32_t)(AB[line_B + 1] * fraction + AB[line_B] * (1 - fraction)), 0, 1023, 0, 1023) + bias, 0, 1023);
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    if (output <= 0 ) output = 0.01;
  }
  if (wipe == false)queue1.unshift(output);
}

void room_fx() {
  while (!queue.isEmpty()) {
    utime = map(level >> 5, 0, 31, 1000, 2500);
    static unsigned int bufferIn = bufferSize, bufferOut = bufferSize - utime;
    buffer[bufferIn] = queue.pop();
    bufferIn++;
    if (bufferIn >= utime) bufferIn = 0;
    bufferOut++;
    if (bufferOut >= utime) bufferOut = 0;
    output = map(((buffer[bufferOut] - bias) + (audioIn - bias)), 0, 2047, 0, 1023) + bias;
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
  }
  if (wipe == false)queue1.unshift(output);
}

void reverplex_fx() {
  while (!queue.isEmpty()) {
    time = map(level >> 5, 0, 31, 900, 1600);
    static long bufferIn = bufferSize, bufferOut = bufferSize - time;
    buffer[bufferIn] = ((queue.pop() - bias) + ((buffer[bufferIn] - bias) >> 1) + bias);
    bufferIn++;
    if (bufferIn >= time) bufferIn = 0;
    bufferOut++;
    if (bufferOut >= time) bufferOut = 0;
    output = map(((buffer[bufferOut] - bias) + (audioIn - bias)), 0, 2047, 0, 1023) + bias;
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
  }
  if (wipe == false)queue1.unshift(output);
}

void reverb_fx() {
  while (!queue.isEmpty()) {
    DV1 = map(level << 2, 0, 4095, 100, 4000);
    DV2 = map(level << 2, 0, 4095, 100, 4000);
    DV3 = map(level << 2, 0, 4095, 150, 4000);
    DV4 = map(level << 2, 0, 4095, 200, 4000);
    DV5 = map(level << 2, 0, 4095, 400, 10000);
    DV6 = map(level << 2, 0, 4095, 200, 4000);

    // One input sample feeds all taps (queue only holds ~1 sample per ISR)
    int32_t xin = queue.pop();
    X1[DC1] = (xin + x1 * X1[DC1] / 1.2);
    X2[DC2] = (xin + x2 * X2[DC2] / 1.4);
    X3[DC3] = (xin * X3[DC3] / 1.6);
    X4[DC4] = (xin * X4[DC4] / 1.8);

    S1 =  y_1 * X1[DC1];
    S2 =  y_2 * X2[DC2];
    S3 =  y_3 * X3[DC3];
    S4 =  y_4 * X4[DC4];

    DC1++; if (DC1 >= DV1) DC1 = 0;
    DC2++; if (DC2 >= DV2) DC2 = 0;
    DC3++; if (DC3 >= DV3) DC3 = 0;
    DC4++; if (DC4 >= DV4) DC4 = 0;

    S5 = (S1 + S2 + S3 + S4) / 4;
    X5[DC7] = (S5 + x3 * X5[DC5]);
    S6 = -x3 * X5[DC7] + X5[DC5];
    DC5++; if (DC5 >= DV5) DC5 = 0;
    DC7++; if (DC7 >= D5) DC7 = 0;
    X6[DC8] = (S6 + x4 * X6[DC6]);
    S7 = constrain(map(((int32_t)(-x4 * X6[DC8] + X6[DC6]) * 1.99), 0, 2047, 0, 1023), 0, 1023);
    DC6++; if (DC6 >= DV6) DC6 = 0;
    DC8++; if (DC8 >= D6) DC8 = 0;
    output = map((S7 - bias) + (audioIn - bias), 0, 2047, 0, 1023) + bias;
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
  }
  if (wipe == false)queue1.unshift(output);
}

void addAverage(int raw_data) {
  bufferAverage[indexAverage] = raw_data;
  indexAverage++;
  if (indexAverage >= bufferSizeAverage) indexAverage = 0;
}

int average() {
  long sum = 0;
  for (int i = 0; i < bufferSizeAverage; i++)
  {
    sum += bufferAverage[i];
  }
  return (int)(sum / bufferSizeAverage);
}

void Encoder() {
  // Falling edge on ENC_A; ENC_B selects direction
  bool readings = digitalRead(ENC_A);
  if (last_readings == true && readings == false) {
    if (digitalRead(ENC_B) == LOW) {
      if (encoder < 33) encoder++;
    } else {
      if (encoder > 0) encoder--;
    }
  }
  last_readings = readings;
}

void Button() {
  // Edge detect only — no delay*() inside the audio ISR
  bool change = digitalRead(button);
  if (change != state) {
    if (change == false) {
      bypass = !bypass;
    }
    state = change;
  }
}

void ext_button() {
  bool change = digitalRead(EXT_CV_BTN);
  if (change != ext_state) {
    if (change == false) {
      ext_on = !ext_on;
    }
    ext_state = change;
  }
}

void control() {
  static  unsigned long debounce = 0;
  static  long debounceTime = 100;
  rec = !digitalRead(recordPin);
  bool check_button = !digitalRead(stopPin);
  if (check_button && button_time >= 1000) {
    button_time = 0;
    looper_stop = true;
  } else {
    looper_stop = false;
  }

  if (recording == true)  digitalWrite(recordLed, HIGH); else digitalWrite(recordLed, LOW);
  if (playback == true) digitalWrite(replayLed, HIGH); else digitalWrite(replayLed, LOW);

  if (!recording && rec && !looper_stop && counterMillis >= debounce) {
    recording = true;
    looper_run = true;
    debounce = counterMillis + debounceTime;
    if (playback) {
      playback = false;
      wipe_loop_req = true; // defer 85k wipe to loop()
    }
    startTime = counterMillis;
  }
  if (recording && !rec && counterMillis >= debounce) { //debounce
    recording = false;
    playback = true;
    looper_run = true;
    debounce = counterMillis + debounceTime;
    position[place] = 0;
    bufferTime = counterMillis - startTime;
    prepBuffer();
  }
  if (looper_stop) {
    loop_bar = 0;
    recording = false;
    playback = false;
    looper_run = false;
    loopOut = bias;
    bufferOutput = bias;
    wipe_loop_req = true; // defer 85k wipe to loop()
  }
}

void wipeBuffer() { //Clear buffer
  for (unsigned long i = 0; i < loopLength; i++) {
    position[i] = 0L;
  }
  place = 0;
}

void prepBuffer() { //Next play
  unsigned long i = 0;
  while (position[i] != 0) {
    position[i] += bufferTime;
    i++;
  }
  place = 0;
}

void save_loop() {
  if (recording) {
    position[place] = counterMillis;
    loopAudio[place] = loopIn;
    place++;
    if (place >= loopLength) {
      recording = false;
      playback = true;
    }
  }
}

void play_loop() {
  if (playback && position[0] != 0L) {
    if (counterMillis >= position[place]) {
      loopOut = loopAudio[place];
      position[place] += bufferTime;
      place++;
      if (position[place] == 0) place = 0;
    }
  }
}

void displayShow() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);
  if (bypass == false) {
    display.setCursor(2, 13);
    display.println(nameEffect[encoder]);
  } else { // bypassed
    cln = true;
    display.setCursor(37, 13);
    display.setTextSize(1);
    display.println("BYPASS");
  }
  if (cln == false) {
    display.setTextColor(WHITE);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextSize(1);
    display.setCursor(106, 13);
    display.print(level >> 4);
  }

  display.fillRect(bar, 30, 128 - bar, 40, BLACK);
  display.fillRect(2, 30, bar, 40, WHITE);
  for (int i = 1; i < 32; i++) {
    display.fillRect(i * 4, 30, 2, 40, BLACK);
  }
  display.display();
}

void logo() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(11, 10);
  display.println("PENELOOPE");
  display.setTextSize(1);
  display.setCursor(10, 35);
  display.println("DIODAC ELECTRONICS");
  display.setTextSize(1);
  display.setCursor(45, 50);
  display.println("REV 2.0");
  display.display();
  delay(4000);
  display.clearDisplay();
}
