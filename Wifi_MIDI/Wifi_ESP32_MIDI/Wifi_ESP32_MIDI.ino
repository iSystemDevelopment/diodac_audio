/*
  Copyright (c) 2026 DIODAC ELECTRONICS / iSystem Development
  Licensed under the MIT License.
  Contact: diodac.electronics@gmail.com

  Wifi_ESP32_MIDI — serial MIDI <-> AppleMIDI (rtpMIDI) bridge
  Target: ESP32-S3 (also builds for other ESP32 variants with pin edits)

  DIN MIDI uses HardwareSerial Serial1 (USB Serial stays free for programming).
  Adjust MIDI_RX_PIN / MIDI_TX_PIN to match your wiring.

  Provisioning: WiFiManager AP "MIDI2NET" (AP password is local-portal only).
  rtpMIDI: session name MIDI2NET, UDP port 5004.
  Status: http://<device-ip>/
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <AppleMIDI.h>
#include <MIDI.h>

// ---------- Board wiring (ESP32-S3 defaults — change to match your PCB) ----------
#ifndef MIDI_RX_PIN
#define MIDI_RX_PIN 18
#endif
#ifndef MIDI_TX_PIN
#define MIDI_TX_PIN 17
#endif
#ifndef MIDI_BAUD
#define MIDI_BAUD 31250
#endif
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN (-1)  // set to a GPIO (e.g. 2) if you have a status LED
#endif

static const char *const DEVICE_NAME = "MIDI2NET";
static const char *const AP_PASSWORD = "!12345678";  // WiFiManager AP only

APPLEMIDI_CREATE_INSTANCE(WiFiUDP, AppleMIDI);

HardwareSerial &MidiSerial = Serial1;
MIDI_CREATE_INSTANCE(HardwareSerial, MidiSerial, MIDI);

WebServer server(80);

static bool rtpConnected = false;
static uint32_t midiInCount = 0;   // DIN -> network
static uint32_t midiOutCount = 0;  // network -> DIN

static void setStatusLed(bool on) {
#if STATUS_LED_PIN >= 0
  digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
#else
  (void)on;
#endif
}

// ---------- DIN MIDI -> AppleMIDI ----------
void OnSerialMidiNoteOn(byte channel, byte note, byte velocity) {
  midiInCount++;
  AppleMIDI.sendNoteOn(note, velocity, channel);
}
void OnSerialMidiNoteOff(byte channel, byte note, byte velocity) {
  midiInCount++;
  AppleMIDI.sendNoteOff(note, velocity, channel);
}
void OnSerialMidiCC(byte controlNumber, byte controlValue, byte channel) {
  midiInCount++;
  AppleMIDI.sendControlChange(controlNumber, controlValue, channel);
}
void OnSerialMidiAfterTouchPoly(byte channel, byte note, byte pressure) {
  midiInCount++;
  AppleMIDI.sendPolyPressure(note, pressure, channel);
}
void OnSerialMidiProgramChange(byte channel, byte number) {
  midiInCount++;
  AppleMIDI.sendProgramChange(number, channel);
}
void OnSerialMidiAfterTouchChannel(byte channel, byte pressure) {
  midiInCount++;
  AppleMIDI.sendAfterTouch(pressure, channel);
}
void OnSerialMidiPitchBend(byte channel, int bend) {
  midiInCount++;
  AppleMIDI.sendPitchBend(bend, channel);
}
void OnSerialMidiTimeCodeQuarterFrame(byte data) {
  midiInCount++;
  AppleMIDI.sendTimeCodeQuarterFrame(data);
}

// ---------- AppleMIDI -> DIN MIDI ----------
void OnAppleMidiNoteOn(byte channel, byte note, byte velocity) {
  midiOutCount++;
  MIDI.sendNoteOn(note, velocity, channel);
}
void OnAppleMidiNoteOff(byte channel, byte note, byte velocity) {
  midiOutCount++;
  MIDI.sendNoteOff(note, velocity, channel);
}
void OnAppleMidiCc(byte controlNumber, byte controlValue, byte channel) {
  midiOutCount++;
  MIDI.sendControlChange(controlNumber, controlValue, channel);
}
void OnAppleMidiReceiveAfterTouchPoly(byte channel, byte note, byte pressure) {
  midiOutCount++;
  MIDI.sendAfterTouch(note, pressure, channel);
}
void OnAppleMidiReceiveProgramChange(byte channel, byte number) {
  midiOutCount++;
  MIDI.sendProgramChange(number, channel);
}
void OnAppleMidiReceiveAfterTouchChannel(byte channel, byte pressure) {
  midiOutCount++;
  MIDI.sendAfterTouch(pressure, channel);
}
void OnAppleMidiReceivePitchBend(byte channel, int bend) {
  midiOutCount++;
  MIDI.sendPitchBend(bend, channel);
}
void OnAppleMidiReceiveTimeCodeQuarterFrame(byte data) {
  midiOutCount++;
  MIDI.sendTimeCodeQuarterFrame(data);
}

void OnAppleMidiConnected(uint32_t /*ssrc*/, char * /*name*/) {
  rtpConnected = true;
  setStatusLed(true);
}
void OnAppleMidiDisconnected(uint32_t /*ssrc*/) {
  rtpConnected = false;
  setStatusLed(false);
}

String SendHTML(uint32_t din_to_net, uint32_t net_to_din) {
  String html;
  html.reserve(1600);
  html += F("<!DOCTYPE html><html><head>");
  html += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  html += F("<title>DIODAC rtpMIDI (ESP32)</title>");
  html += F("<style>");
  html += F("body{font-family:Helvetica,Arial,sans-serif;text-align:center;margin:40px;color:#333}");
  html += F("h1{margin-bottom:8px} .ok{color:#0a7} .bad{color:#c33}");
  html += F("p{font-size:1.15rem;margin:8px}");
  html += F("</style><script>");
  html += F("setInterval(function(){var x=new XMLHttpRequest();");
  html += F("x.onload=function(){if(x.status==200)");
  html += F("document.getElementById('webpage').innerHTML=x.responseText;};");
  html += F("x.open('GET','/status',true);x.send();},500);");
  html += F("</script></head><body><div id=\"webpage\">");
  html += F("<h1>DIODAC rtpMIDI</h1>");
  html += F("<p>Platform: ESP32-S3</p><p>Device: MIDI2NET</p><p>WiFi: ");
  html += WiFi.localIP().toString();
  html += F("</p><p>Session: <span class=\"");
  html += rtpConnected ? F("ok\">connected") : F("bad\">waiting");
  html += F("</span></p>");
  html += F("<p>DIN → Net: ");
  html += String(din_to_net);
  html += F("</p><p>Net → DIN: ");
  html += String(net_to_din);
  html += F("</p></div></body></html>");
  return html;
}

String SendStatusFragment() {
  String frag;
  frag.reserve(480);
  frag += F("<h1>DIODAC rtpMIDI</h1>");
  frag += F("<p>Platform: ESP32-S3</p><p>Device: MIDI2NET</p><p>WiFi: ");
  frag += WiFi.localIP().toString();
  frag += F("</p><p>Session: <span class=\"");
  frag += rtpConnected ? F("ok\">connected") : F("bad\">waiting");
  frag += F("</span></p>");
  frag += F("<p>DIN → Net: ");
  frag += String(midiInCount);
  frag += F("</p><p>Net → DIN: ");
  frag += String(midiOutCount);
  frag += F("</p>");
  return frag;
}

void handle_OnConnect() {
  server.send(200, "text/html", SendHTML(midiInCount, midiOutCount));
}
void handle_Status() {
  server.send(200, "text/html", SendStatusFragment());
}
void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

void setupSerialMidiHandlers() {
  MIDI.setHandleNoteOn(OnSerialMidiNoteOn);
  MIDI.setHandleNoteOff(OnSerialMidiNoteOff);
  MIDI.setHandleControlChange(OnSerialMidiCC);
  MIDI.setHandleAfterTouchPoly(OnSerialMidiAfterTouchPoly);
  MIDI.setHandleProgramChange(OnSerialMidiProgramChange);
  MIDI.setHandleAfterTouchChannel(OnSerialMidiAfterTouchChannel);
  MIDI.setHandlePitchBend(OnSerialMidiPitchBend);
  MIDI.setHandleTimeCodeQuarterFrame(OnSerialMidiTimeCodeQuarterFrame);
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.turnThruOff();
}

void setupAppleMidiHandlers() {
  AppleMIDI.OnConnected(OnAppleMidiConnected);
  AppleMIDI.OnDisconnected(OnAppleMidiDisconnected);
  AppleMIDI.OnReceiveNoteOn(OnAppleMidiNoteOn);
  AppleMIDI.OnReceiveNoteOff(OnAppleMidiNoteOff);
  AppleMIDI.OnReceiveControlChange(OnAppleMidiCc);
  AppleMIDI.OnReceiveAfterTouchPoly(OnAppleMidiReceiveAfterTouchPoly);
  AppleMIDI.OnReceiveProgramChange(OnAppleMidiReceiveProgramChange);
  AppleMIDI.OnReceiveAfterTouchChannel(OnAppleMidiReceiveAfterTouchChannel);
  AppleMIDI.OnReceivePitchBend(OnAppleMidiReceivePitchBend);
  AppleMIDI.OnReceiveTimeCodeQuarterFrame(OnAppleMidiReceiveTimeCodeQuarterFrame);
  AppleMIDI.begin(DEVICE_NAME);
}

void setup() {
  // USB serial optional (debug / Arduino Serial Monitor) — not used for DIN MIDI
  Serial.begin(115200);
  delay(200);

#if STATUS_LED_PIN >= 0
  pinMode(STATUS_LED_PIN, OUTPUT);
  setStatusLed(false);
#endif

  // DIN MIDI on UART1
  MidiSerial.begin(MIDI_BAUD, SERIAL_8N1, MIDI_RX_PIN, MIDI_TX_PIN);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_NAME);

  WiFiManager wifiManager;
  wifiManager.setBreakAfterConfig(true);
  // wifiManager.resetSettings();  // uncomment once to wipe stored credentials

  if (!wifiManager.autoConnect(DEVICE_NAME, AP_PASSWORD)) {
    delay(2000);
    ESP.restart();
  }

  server.on("/", handle_OnConnect);
  server.on("/status", handle_Status);
  server.onNotFound(handle_NotFound);
  server.begin();

  setupSerialMidiHandlers();
  setupAppleMidiHandlers();

  Serial.print(F("rtpMIDI ready — IP "));
  Serial.print(WiFi.localIP());
  Serial.println(F(":5004 session MIDI2NET"));
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    ESP.restart();
  }

  AppleMIDI.read();
  MIDI.read();
  server.handleClient();
}
