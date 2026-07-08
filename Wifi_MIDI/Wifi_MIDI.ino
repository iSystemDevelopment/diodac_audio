/*
  Copyright (c) 2026 DIODAC ELECTRONICS / iSystem Development
  Licensed under the MIT License.
  Contact: diodac.electronics@gmail.com
*/

// DIODAC ELECTRONICS
#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include "AppleMidi.h"
#include <MIDI.h>
#include <WiFiUdp.h>
APPLEMIDI_CREATE_INSTANCE(WiFiUDP, AppleMIDI);
MIDI_CREATE_DEFAULT_INSTANCE();

ESP8266WebServer server(80);
unsigned long t0 = millis();
bool isConnected = false;

const char* ssid = "MIDI2NET";  // Device AP/AppleMIDI name during provisioning.
const char* password = "";  // Unused; WiFiManager handles provisioning. Do not hardcode real network passwords.

byte midi_in;
byte midi_out;

void OnSerialMidiNoteOn(byte channel, byte note, byte velocity) {
  AppleMIDI.sendNoteOn(note, velocity, channel); //send Note On to rtpMIDI
}
void OnSerialMidiNoteOff(byte channel, byte note, byte velocity) {
  AppleMIDI.sendNoteOff(note, velocity, channel); //send Note OFF to rtpMIDI
}
void OnSerialMidiCC (byte controlNumber, byte controlValue, byte channel) {
  AppleMIDI.sendControlChange(controlNumber, controlValue, channel); //send Note OFF to rtpMIDI
}
void OnSerialMidiAfterTouchPoly(byte channel, byte note, byte pressure) {
  AppleMIDI.sendPolyPressure(note, pressure, channel);
}
void OnSerialMidiProgramChange(byte channel, byte number) {
  AppleMIDI.sendProgramChange(number, channel);
}
void OnSerialMidiAfterTouchChannel (byte channel, byte pressure) {
  AppleMIDI.sendAfterTouch(pressure, channel);
}
void OnSerialMidiPitchBend (byte channel, int bend) {
  AppleMIDI.sendPitchBend(bend, channel);
}
void OnSerialMidiTimeCodeQuarterFrame (byte data) {
  AppleMIDI.sendTimeCodeQuarterFrame(data);
}

void setup() {
  Serial.begin(31250);

 // Serial.println();
  WiFiManager wifiManager;
  wifiManager.setBreakAfterConfig(true);
  //reset settings - for testing
  //wifiManager.resetSettings();
  WiFi.mode(WIFI_STA);
  WiFi.hostname("MIDI2NET");

  // WiFiManager starts the MIDI2NET provisioning AP; do not put real WiFi credentials in source.
  if (!wifiManager.autoConnect("MIDI2NET", "!12345678")) {
   // Serial.println("Try again...");
    delay(3000);
    ESP.reset();
    delay(5000);
  }
  //Serial.println("Connected! :)");
  //Serial.println(F("OK, now make sure you an rtpMIDI session that is Enabled"));
  //Serial.print(F("Add device named MIDI2NET with Host/Port "));
  //Serial.println(WiFi.localIP());
  //Serial.println(F(":5004"));
  //Serial.println(F("Then press the Connect button"));
  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();
  //Serial.println("HTTP server started");
  //Serial.println();


  MIDI.setHandleNoteOn(OnSerialMidiNoteOn);           // handle serial MIDI note ON, Put only the name of the function,
  MIDI.setHandleNoteOff(OnSerialMidiNoteOff);         // handle serial MIDI note OFF
  MIDI.setHandleControlChange(OnSerialMidiCC);        // handle serial MIDI CC
  MIDI.setHandleAfterTouchPoly(OnSerialMidiAfterTouchPoly);
  MIDI.setHandleProgramChange(OnSerialMidiProgramChange);
  MIDI.setHandleAfterTouchChannel(OnSerialMidiAfterTouchChannel);
  MIDI.setHandlePitchBend(OnSerialMidiPitchBend);
  MIDI.setHandleTimeCodeQuarterFrame(OnSerialMidiTimeCodeQuarterFrame);
  //MIDI.begin(MIDI_CHANNEL_OMNI); 
   MIDI.begin();  
  // start serial MIDI
  MIDI.turnThruOff();

  AppleMIDI.OnConnected(OnAppleMidiConnected);
  AppleMIDI.OnDisconnected(OnAppleMidiDisconnected);
  AppleMIDI.OnReceiveNoteOn(OnAppleMidiNoteOn);
  AppleMIDI.OnReceiveNoteOff(OnAppleMidiNoteOff);
  AppleMIDI.OnReceiveControlChange(OnAppleMidiCc);    // handle rtpMIDI CC
  AppleMIDI.OnReceiveAfterTouchPoly(OnAppleMidiReceiveAfterTouchPoly);
  AppleMIDI.OnReceiveProgramChange(OnAppleMidiReceiveProgramChange);
  AppleMIDI.OnReceiveAfterTouchChannel(OnAppleMidiReceiveAfterTouchChannel);
  AppleMIDI.OnReceivePitchBend(OnAppleMidiReceivePitchBend);
  AppleMIDI.OnReceiveTimeCodeQuarterFrame(OnAppleMidiReceiveTimeCodeQuarterFrame);
  AppleMIDI.begin("MIDI2NET");

}

void loop() {
  
  AppleMIDI.read();

  MIDI.read();

  server.handleClient();
}
void OnAppleMidiConnected(uint32_t ssrc, char* name) {
  isConnected  = true;
  //Serial.print(F("Connected to session "));
 // Serial.println(name);
}

// -----------------------------------------------------------------------------
// rtpMIDI session. Device disconnected
// -----------------------------------------------------------------------------
void OnAppleMidiDisconnected(uint32_t ssrc) {
  isConnected  = false;
 // Serial.println(F("Disconnected"));
}
void handle_OnConnect() {

  midi_in = random(1, 128); // Gets the values of the temperature
  midi_out = random(1, 255);; // Gets the values of the humidity
  server.send(200, "text/html", SendHTML( midi_in, midi_out));
}
void OnAppleMidiNoteOn(byte channel, byte note, byte velocity) {
  
  MIDI.sendNoteOn(note, velocity, channel); //send Note ON to serial MIDI
}
void OnAppleMidiNoteOff(byte channel, byte note, byte velocity) {
  MIDI.sendNoteOff(note, velocity, channel); //send Note Off to serial MIDI
}
void OnAppleMidiCc(byte controlNumber, byte controlValue, byte channel) {
  MIDI.sendControlChange(controlNumber, controlValue, channel); //send Note Off to serial MIDI
}
void OnAppleMidiReceiveAfterTouchPoly(byte channel, byte note, byte pressure) {
  MIDI.sendAfterTouch(note, pressure, channel);
}
void OnAppleMidiReceiveProgramChange(byte channel, byte number) {
  MIDI.sendProgramChange(number, channel);
}
void OnAppleMidiReceiveAfterTouchChannel(byte channel, byte pressure) {
  MIDI.sendAfterTouch(pressure, channel);
}
void OnAppleMidiReceivePitchBend(byte channel, int bend) {
  MIDI.sendPitchBend(bend, channel);
}
void OnAppleMidiReceiveTimeCodeQuarterFrame(byte data) {
  MIDI.sendTimeCodeQuarterFrame(data);
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML(byte midi_input, byte midi_output) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<script>\n";
  ptr += "setInterval(loadDoc,200);\n";
  ptr += "function loadDoc() {\n";
  ptr += "var xhttp = new XMLHttpRequest();\n";
  ptr += "xhttp.onreadystatechange = function() {\n";
  ptr += "if (this.readyState == 4 && this.status == 200) {\n";
  ptr += "document.getElementById(\"webpage\").innerHTML =this.responseText}\n";
  ptr += "};\n";
  ptr += "xhttp.open(\"GET\", \"/\", true);\n";
  ptr += "xhttp.send();\n";
  ptr += "}\n";
  ptr += "</script>\n";
  //ptr +="<meta http-equiv="refresh" content="1" >\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Diodac_rtpMIDI_NET</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  ptr += "p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<div id=\"webpage\">\n";
  ptr += "<h1>Diodac_rtpMIDI_NET</h1>\n";

  ptr += "<p>MIDI IN: ";
  ptr += (int)midi_input;
  ptr += " </p>";
  ptr += "<p>MIDI OUT: ";
  ptr += (int)midi_out;
  ptr += "</p>";

  ptr += "</div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
