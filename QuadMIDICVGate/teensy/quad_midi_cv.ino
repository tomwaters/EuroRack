#include <Adafruit_MCP4728.h>
#include <Wire.h>
#include <Shifty.h>
#include <MIDI.h>
#include "config.h"

Adafruit_MCP4728 mcp;
Shifty myreg;
elapsedMillis sinceClock;
MIDI_CREATE_INSTANCE(HardwareSerial, MIDI_SERIAL, MIDI);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting");
  // CV/DAC
  if (!mcp.begin()) {
    Serial.println("Failed to find MCP4728 chip");
    while (1) {
      delay(10);
    }
  }

  // Gate/74HC595
  myreg.setBitCount(8);
  myreg.setPins(11, 13, 8);
  resetCVGate();
  
  usbMIDI.setHandleNoteOff(onNoteOff);
  usbMIDI.setHandleNoteOn(onNoteOn);
  usbMIDI.setHandleClock(onClock);
  usbMIDI.setHandleStop(onStop);
  usbMIDI.setHandleContinue(onContinue);

  MIDI.begin();
}

void loop() {
  // MIDI thru usb to din
  if(usbMIDI.read()) {
    byte data1 = usbMIDI.getData1();
    byte data2 = usbMIDI.getData2();
    byte type = usbMIDI.getType();
    byte channel = usbMIDI.getChannel();
    
    MIDI.send(MIDI.getTypeFromStatusByte(type),
      data1,
      data2,
      MIDI.getChannelFromStatusByte(channel));
  }

  if(sinceClock > CLOCK_MS) {
    myreg.writeBit(CLOCK_PIN, LOW);
  }
}

void resetCVGate() {
  for(int g=1; g<5; g++) {
    myreg.writeBit(g, LOW);
  }
  mcp.setChannelValue(MCP4728_CHANNEL_A, 0, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  mcp.setChannelValue(MCP4728_CHANNEL_B, 0, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  mcp.setChannelValue(MCP4728_CHANNEL_C, 0, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  mcp.setChannelValue(MCP4728_CHANNEL_D, 0, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
}

int getGatePin(byte channel) {
  if(channel == CV_CHANNEL_1)
    return 1;
  else if(channel == CV_CHANNEL_2)
    return 2;
  else if(channel == CV_CHANNEL_3)
    return 3;
  else if(channel == CV_CHANNEL_4)
    return 4;
  else
    return -1;
}

void onNoteOn(byte channel, byte note, byte velocity) {
  // Check if we are configured to use this channel
  MCP4728_channel_t cv_chan = MCP4728_CHANNEL_A;
  bool found_cv = true;  
  if(channel == CV_CHANNEL_1)
    cv_chan = MCP4728_CHANNEL_A;
  else if(channel == CV_CHANNEL_2)
    cv_chan = MCP4728_CHANNEL_B;
  else if(channel == CV_CHANNEL_3)
    cv_chan = MCP4728_CHANNEL_C;
  else if(channel == CV_CHANNEL_4)
    cv_chan = MCP4728_CHANNEL_D;
  else
    found_cv = false;
    
  if(found_cv) {
    uint16_t  cv_val = (note - ZERO_V_MIDI_NOTE) * 83.3333333;
    mcp.setChannelValue(cv_chan, cv_val, MCP4728_VREF_INTERNAL, MCP4728_GAIN_2X);
  }

  // Gate output
  int gate_pin = getGatePin(channel);
  if(gate_pin > 0) {
    myreg.writeBit(gate_pin, HIGH);
  }
}

void onNoteOff(byte channel, byte note, byte velocity) {
  // Gate output
  int gate_pin = getGatePin(channel);
  if(gate_pin > 0) {
    myreg.writeBit(gate_pin, LOW);
  }
}

uint clockCount = 0;
bool resetClock = false;
void onClock() {
  if(clockCount == 0 || resetClock) {
    myreg.writeBit(CLOCK_PIN, HIGH);
    sinceClock = 0;
    clockCount = 0;
    resetClock = false;
  }
  
  clockCount++;
  if(clockCount >= CLOCK_COUNT) {
    clockCount = 0;
  }
}

// Restart clock at 0 unless a Continue arrives before the next clock
// TODO: use song position to figure out where we are when pausing/resuming
void onStop() {
    resetClock = true;
}
void onContinue() {
    resetClock = false;
}
