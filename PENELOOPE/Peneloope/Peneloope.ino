/*
  Copyright (c) 2026 DIODAC ELECTRONICS / iSystem Development
  Licensed under the MIT License.
  Contact: diodac.electronics@gmail.com

     MULTI EFFECT DSP WITH LOOPER
            "PENELOOPE"
         DIODAC ELECTRONICS
              REV. 2.0  
*/
IntervalTimer timer;
#include <SPI.h>
#include <Wire.h>
#include <ADC.h>
#include <ADC_util.h>
ADC *adc = new ADC();
#include <CircularBuffer.h>
CircularBuffer<int32_t, 128> queue;
CircularBuffer<int32_t, 128> queue1;
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

//IO
#define led  4
#define A  5
#define B  6
#define button  7
#define dac_latch 9
#define dac_cs 10

//Display settings
#define OLED_ADDR   0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET  -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String nameEffect[34] = {"CLEAN", "DISTRO", "FUZZLE",
                         "CRUSH", "OVER", "DETUNE",
                         "PITCH-UP", "PITCH-DN", "FLANGE",
                         "PITCHDEL", "DELAY", "DELONG", "ECHO",
                         "ECHOLONG", "REVERSE", "TREMOLO",
                         "LFO", "PENTAVER", "HEXAVER",
                         "SEPTAVER", "RIDOO", "WOODOO",
                         "MODULATE", "WOBBLE", "TRIDULATE",
                         "SINE", "TRIANGLE", "CHORUS",
                         "Q-RES", "DEEP-WAH", "WAH-WAH",
                         "ROOM", "REVERPLEX", "REVERB"
                        };

//Multipicator
inline uint32_t M32x16(uint32_t x, uint32_t y) {
  return (uint32_t)(((uint32_t)x) * ((uint32_t)y) >> 16);
}

//Sound Level bar variables
#define numGrains 128
int FASTRUN image[numGrains];
int FASTRUN real[numGrains];
volatile int bar = 0;

//Main variables
elapsedMillis counter;
elapsedMillis count;
elapsedMillis button_time;
elapsedMillis counterMillis;
elapsedMillis refresh;
boolean last_readings = false;
boolean state = true;
boolean on = false;
boolean ext_state = true;
boolean ext_on = false;
volatile boolean cln = false;
#define bias 512
boolean active = false;
int32_t output_vol = 0, loop_vol = 0, mix_channel = 0, master = 0;
volatile static byte last_encoder = 0, encoder = 0;
int32_t audioIn = 0, audioOut = 0, main_out = 0, dry = 0, input = bias, output = bias, bufferOutput = bias;
int fx_pot = 0, external = 0;
volatile int level = 0;

//LPF variables
int32_t x_1 = 0, d_1 = 0;
long f_1 = 0;

//Main buffers
#define bufferSizeAverage  32
int FASTRUN bufferAverage[bufferSizeAverage];
int indexAverage = 0;
boolean wipe = false;
#define bufferSize 48000
int FASTRUN buffer[bufferSize];

//Oscillators
#define points 2047
unsigned int FASTRUN modulation[points];
unsigned int FASTRUN sine[points];
unsigned int FASTRUN cosine[points];

//Effect variables
#define ShiftBits_amplitude 2
#define AmplitudeMax 0x0200
int distortion = 0, fuzz = 0, bit_crush = 0, amplitude = 0;
double threshold = 0.2, in_0 = 0;

//Flange & Shifters
#define MIN 2 //~60us distance
#define MAX 400 //~ 8.5ms distance
#define SIZE 400
#define pitch 11700
byte dir = 1;
unsigned int utime = 0, offset = 0, increment = 0, divider = 0, distance = 0, fractional = 0x00;
int speed = 0, sample = 0, time = 0;
int32_t resultA = 0, resultB = 0, outputA = 0, outputB = 0;

//Modulators & Wobbies++
#define space 350
#define maximum 300
unsigned int wave = bias;
int modulation_in = 0;
int FASTRUN AB[maximum];
int32_t delay_line = 0, line_A = 0, line_B = 0, calculator = 0, fraction = 0;
double down = -0.5, down1 = -0.98, down2 = -0.8;
double up =  0.5, up1 = 0.98, up2 = 0.8;

//Reverb+++++++++++++++
#define D1  4000
#define D2  4000
#define D3  4000
#define D4  4000
#define D5  10000
#define D6  4000
int  DV1 = 100;
int  DV2 = 100;
int  DV3 = 150;
int  DV4 = 200;
int  DV5 = 400;
int  DV6 = 200;
double  x1 = 0.69, x2 = 0.64, x3 = 0.59, x4 = 0.54;
double  y_1 = 0.99 , y_2 = 0.94 , y_3 = 0.89 , y_4 = 0.84;
int FASTRUN X1[D1];
int FASTRUN X2[D2];
int FASTRUN X3[D3];
int FASTRUN X4[D4];
int FASTRUN X5[D5];
int FASTRUN X6[D6];
int32_t  S1 = 0, S2 = 0, S3 = 0, S4 = 0, S5 = 0, S6 = 0, S7 = 0;
long  DC1 = 0, DC2 = 0, DC3 = 0, DC4 = 0, DC5 = 0, DC6 = 1, DC7 = 1, DC8 = 0;
//++++++++++++++++++++

//Looper++++++++++++++
#define loopLength 85000 //~8.5 sec audio recording
#define recordLed 0
#define replayLed 1
#define recordPin 2
#define stopPin   3
int32_t loopIn = 0, loopOut = bias;
boolean rec = false, stop = false, ledRec = false, ledPlay = false, looper_run = false;
unsigned long startTime = 0, endTime = 0, bufferTime = 0;
boolean recording = false, playback = false;
unsigned long place;
volatile int loop_bar;
unsigned long DMAMEM position[loopLength];
unsigned short DMAMEM loopAudio[loopLength];
//++++++++++++++++++++
void DAC(int32_t data) {
  //20MHz for MCP4921
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE3));
  digitalWriteFast(dac_cs, LOW);
  SPI.transfer16((data & 0x0FFF) | 0x7000);
  SPI.transfer16(data & 0xFFF);
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
  pinMode(14, INPUT_DISABLE);
  pinMode(15, INPUT_DISABLE);
  pinMode(16, INPUT_DISABLE);
  pinMode(17, INPUT_DISABLE);
  pinMode(20, INPUT_DISABLE);
  pinMode(21, INPUT_DISABLE);
  pinMode(22, INPUT_PULLUP);
  pinMode(23, INPUT_DISABLE);
  pinMode(A, INPUT_PULLUP);
  pinMode(B, INPUT_PULLUP);
  pinMode(button, INPUT_PULLUP);
  pinMode(led, OUTPUT);
  pinMode(recordLed, OUTPUT);
  pinMode(replayLed, OUTPUT);
  digitalWrite(recordLed, LOW);
  digitalWrite(replayLed, LOW);
  pinMode(recordPin, INPUT_PULLUP);
  pinMode(stopPin, INPUT_PULLUP);
  on = false;
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  logo();
  SPI.usingInterrupt(60);
  SPI.begin();
  timer.begin(effect, 60);
  timer.priority(128);
  for (unsigned int i = 0; i < bufferSize; i++) {
    buffer[i] = bias;
  }
  modulation_generator();
  sine_generator();
  cosine_generator();
  //ADC0
  adc->adc0->setAveraging(16);
  adc->adc0->setResolution(12); //resolution
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::MED_SPEED);
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED);
  adc->adc0->setReference(ADC_REFERENCE::REF_3V3);
  //ADC1
  adc->adc0->setAveraging(16);
  adc->adc1->setResolution(10); //resolution
  adc->adc1->setConversionSpeed(ADC_CONVERSION_SPEED::MED_SPEED); // change the conversion speed
  adc->adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED); // change the sampling speed
  adc->adc1->setReference(ADC_REFERENCE::REF_3V3);
}

void loop() {
  if (counter >= 64) {//Display refresh 64ms
    counter = 0;
    displayShow();
  }
}

void modulation_generator() {
  for (int i = 0; i < points; i++)
    modulation[i] = (0.99 * cos(((2.0 * PI) / points) * i));
}

void sine_generator() {
  for (int i = 0; i < points; i++)
    sine[i] = (((0.99 + sin(((2.0 * PI) / points) * i)) * points) / 2);
}

void cosine_generator() {
  for (int i = 0; i < points; i++)
    cosine[i] = (((0.99 + cos(((2.0 * PI) / points) * i)) * points) / 2);
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

void buffclr2() {
  while (!queue.isEmpty()) {
    for (int i = 0; i < 4000; i++) {
      X1[i] = (queue.pop() + x1 * X1[i] / 1.2);
      X2[i] = (queue.pop() + x2 * X2[i] / 1.4);
      X3[i] = (queue.pop() * X3[i] / 1.6);
      X4[i] = (queue.pop() * X4[i] / 1.8);
    }
  }
}

void effect() {
  audioIn = adc->adc1->analogRead(A2);
  audioIn = map(audioIn - bias, 0, 1023, 0, 1023) + bias;
  if (audioIn == bias) active = true; else active = false;
  queue.unshift(audioIn);

  if (looper_run == false || on == true) {
    for (int i = 0; i < numGrains; i++)
    {
      real[i] = (audioIn - bias) >> 1;
      image[i] = 0;
      bar = sqrt(real[i] * real[i]) + (image[i] * image[i]);
    }
  } else bar = map(place / 100, 0, 850, 0, 127);

  Button();

  if (on == false) {//Bypas false
    Encoder();
    control();
    save_loop();
    play_loop();
    ext_button();

    if (refresh >= 4) {
      refresh = 0;
      addAverage(adc->adc0->analogRead(A9));
      fx_pot = map(average(), 0, 4095, 0, 1023);
      external = constrain(adc->adc1->analogRead(A7), 0, 1023);
      loop_vol = adc->adc1->analogRead(A0);
      output_vol = adc->adc1->analogRead(A1);
      master = adc->adc1->analogRead(A3);
      mix_channel = adc->adc1->analogRead(A6);

      if (encoder == 0) cln = true; else cln = false;
      if (ext_on == true)level = external, digitalWrite(led, HIGH); else level = fx_pot, digitalWrite(led, LOW);
      if (!digitalRead(A) || !digitalRead(B)) wipe = true, bufferOutput = bias, audioOut = bias, main_out = bias, output = bias, input = bias; else wipe = false;
      if (wipe == true && encoder == 9) {
        buffclr();
      } else if (wipe == true && encoder == 10) {
        buffclr();
      } else if (wipe == true && encoder == 11) {
        buffclr();
      } else if (wipe == true && encoder == 12) {
        buffclr();
      } else if (wipe == true && encoder == 13) {
        buffclr();
      } else if (wipe == true && encoder == 14) {
        buffclr();
      } else if (wipe == true && encoder == 20) {
        buffclr1();
      } else if (wipe == true && encoder == 21) {
        buffclr1();
      } else if (wipe == true && encoder == 22) {
        buffclr1();
      } else if (wipe == true && encoder == 26) {
        buffclr1();
      } else if (wipe == true && encoder == 27) {
        buffclr1();
      } else if (wipe == true && encoder == 28) {
        buffclr1();
      } else if (wipe == true && encoder == 29) {
        buffclr1();
      } else if (wipe == true && encoder == 30) {
        buffclr1();
      } else if (wipe == true && encoder == 31) {
        buffclr();
      } else if (wipe == true && encoder == 32) {
        buffclr();
      } else if (wipe == true && encoder == 33) {
        buffclr2();
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

    loopIn = constrain(map(output - bias, 0, 1023, 0, 1023) + bias, 0, 1023);
    while (!queue1.isEmpty()) {
      bufferOutput = queue1.pop();
    }
    //Mixer audio
    bufferOutput = map(bufferOutput - bias, 0, 1023, 0, output_vol) + bias;
    loopOut = map(loopOut - bias, 0, 1023, 0, loop_vol) + bias;
    audioOut = constrain(((0.75 * (bufferOutput - bias) + 0.75 * (loopOut - bias)) + bias), 0, 1023);
    audioOut = map(audioOut - bias, 0, 1023, 0, 1023) + bias;
    dry = map(audioIn - bias, 0, 1023, 0,  mix_channel) + bias;
    main_out = constrain(((0.75 * (audioOut - bias) + 0.75 * (dry - bias)) + bias), 0, 1023);
    main_out = map(main_out - bias, 0, 1023, 0, master) + bias;
    main_out = map(main_out - bias, 0, 1023, 0, 1023) + bias;
    DAC(main_out >> 1);
  } else if (on == true) DAC(audioIn >> 1);
}

void clean_fx () {
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
  while (!queue.isEmpty()) {
    static int locationIn = SIZE, locationOut = SIZE - fractional;
    buffer[locationIn] = queue.pop() * 3.01;
    locationIn++;
    if (locationIn >= SIZE) locationIn = 0;
    locationOut = locationIn - (fractional >> 8);
    if (locationOut < 0) locationOut += SIZE;
    outputA = buffer[locationOut] + queue.pop();
    locationOut -= 1;
    if (locationOut < 0) locationOut += SIZE;
    outputB = buffer[locationOut] + queue.pop();
    resultA = M32x16(outputA, ((0xff - (fractional & 0x00ff)) << 7));
    resultB = M32x16(outputB, ((fractional & 0x00ff) << 7));
    output = constrain(map(resultA += resultB, 0, 2047, 0, 1023), 0, 1023) ;
    output = map(output - bias, 0, 1023, 0, 1023) + bias;
    int shift = level >> 6;
    if (shift >= 11) shift = 11;
    if (dir) {
      if ((fractional >> 8) >= MAX) dir = 0;
      fractional += (1 + shift);
    }
    else {
      if ((fractional >> 8) <= MIN) dir = 1;
      fractional -= (1 + shift);
    }
  }
  if (wipe == false)queue1.unshift(output);
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

    X1[DC1] = (queue.pop() + x1 * X1[DC1] / 1.2);
    X2[DC2] = (queue.pop() + x2 * X2[DC2] / 1.4);
    X3[DC3] = (queue.pop() * X3[DC3] / 1.6);
    X4[DC4] = (queue.pop() * X4[DC4] / 1.8);

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
  boolean readings = digitalRead(A);
  if ((last_readings == true) && (readings == false))
  {
    if (digitalRead(B) == LOW)
    {
      encoder = encoder + 1.0;
    } else
    {
      encoder = encoder - 1.0;
    }
    encoder = min(encoder, 33);
    encoder = max(encoder, 0);
    encoder = constrain(encoder, 0, 33);
  }
  last_readings = readings;
}

void Button() {
  boolean change = digitalRead(button);
  if (change == false && change != state)
  {
    on = ! on;
    state = change;
    delayMicroseconds(3000);
  }
  if (change == true && change != state)
  {
    state = change;
    delayMicroseconds(3000);
  }
}

void ext_button() {
  boolean change = digitalRead(22);
  if (change == false && change != ext_state)
  {
    ext_on = ! ext_on;
    ext_state = change;
    delayMicroseconds(3000);
  }
  if (change == true && change != ext_state)
  {
    ext_state = change;
    delayMicroseconds(3000);
  }
}

void control() {
  static  unsigned long debounce = 0;
  static  long debounceTime = 100;
  rec = !digitalRead(recordPin);
  boolean check_button = !digitalRead(stopPin);
  if (check_button && button_time >= 1000) {
    button_time = 0;
    stop = true;
  } else stop = false;

  if (recording == true)  digitalWrite(recordLed, HIGH); else digitalWrite(recordLed, LOW);
  if (playback == true) digitalWrite(replayLed, HIGH); else digitalWrite(replayLed, LOW);

  if (!recording && rec && !stop && counterMillis >= debounce) {
    recording = true;
    looper_run = true;
    debounce = counterMillis + debounceTime;
    if (playback) {
      playback = false;
      wipeBuffer();
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
  if (stop) {
    loop_bar = 0;
    recording = false;
    playback = false;
    looper_run = false;
    loopOut = bias;
    bufferOutput = bias;
    wipeBuffer();
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
  if (on == false) {
    display.setCursor(2, 13);
    display.println(nameEffect[encoder]);
  } else if (on == true) {
    cln = true;
    display.setCursor(37, 13);
    display.setTextSize(1);
    display.println("BYPAS");
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
