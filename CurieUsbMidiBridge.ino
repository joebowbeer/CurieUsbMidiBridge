/* Bridge from USB MIDI to BLE MIDI for Arduino 101
 *
 * Components:
 *
 * 1. Hobbytronics USB MIDI Host (5v)
 * 2. Arduino 101 (3.3v)
 *
 * Connections:
 *
 * Hobbytronics 5.5v
 * Hobbytronics GND
 * Hobbytronics TX -> Arduino101 RX (D0)
 */

#include <MIDI.h>
#include <CurieBLE.h>
#include "BleMidiEncoder.h"

//MIDI_CREATE_DEFAULT_INSTANCE();
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

#define LED_PIN LED_BUILTIN
#define LED_ACTIVE HIGH

// BLE MIDI
BLEService midiSvc("03B80E5A-EDE8-4B33-A751-6CE34EC4C700");
BLECharacteristic midiChar("7772E5DB-3868-4112-A1A9-F2669D106BF3",
    BLEWrite | BLEWriteWithoutResponse | BLENotify | BLERead,
    BLE_MIDI_PACKET_SIZE);

class CurieBleMidiEncoder: public BleMidiEncoder {
  boolean setValue(const unsigned char value[], unsigned char length) {
    return midiChar.setValue(value, length);
  }
};

CurieBleMidiEncoder encoder;
boolean connected;

void setup() {
  connected = false;
  pinMode(LED_PIN, OUTPUT);
  displayConnectionState();
  MIDI.begin(MIDI_CHANNEL_OMNI);
  setupBle();
}

void loop() {
  BLE.poll();
  connected = !!BLE.central();
  displayConnectionState();
  while (!encoder.isFull() && MIDI.read() && connected) {
    dispatch();
  }
  encoder.sendMessages();
}

// returns false if encoder failed due to overflow 
boolean dispatch() {
  switch (MIDI.getType()) {
    case midi::NoteOff:
    case midi::NoteOn:
    //case midi::AfterTouchPoly:
    //case midi::ControlChange:
    //case midi::PitchBend:
      return encoder.appendMessage(
          MIDI.getType() | MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());

    case midi::ProgramChange:
    //case midi::AfterTouchChannel:
      return encoder.appendMessage(
          MIDI.getType() | MIDI.getChannel(), MIDI.getData1());
  }
  return true;
}

void displayConnectionState() {
  // LED is lit until we're connected
  digitalWrite(LED_PIN, connected ? !LED_ACTIVE : LED_ACTIVE);
}

void setupBle() {
  BLE.begin();

  BLE.setConnectionInterval(6, 12); // 7.5 to 15 millis

  // set the local name peripheral advertises
  BLE.setLocalName("CurieBLE");

  // set the UUID for the service this peripheral advertises
  BLE.setAdvertisedServiceUuid(midiSvc.uuid());

  // add service and characteristic
  BLE.addService(midiSvc);
  midiSvc.addCharacteristic(midiChar);

  // set an initial value for the characteristic
  encoder.sendMessage(0, 0, 0);

  BLE.advertise();
}

