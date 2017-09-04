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
 * Hobbytronics TX -> Arduino101 D0 (RX)
 */

#include <CurieBLE.h>
#include <MIDI.h>

//MIDI_CREATE_DEFAULT_INSTANCE();
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

#define LED_PIN LED_BUILTIN
#define LED_ACTIVE HIGH

#define BLE_PACKET_SIZE 20

// BLE MIDI
BLEService midiSvc("03B80E5A-EDE8-4B33-A751-6CE34EC4C700");
BLECharacteristic midiChar("7772E5DB-3868-4112-A1A9-F2669D106BF3",
    BLEWrite | BLEWriteWithoutResponse | BLENotify | BLERead, BLE_PACKET_SIZE);

boolean connected;

uint8_t midiData[BLE_PACKET_SIZE];
int byteOffset = 0;
uint8_t lastStatus;
uint16_t lastTime;

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
  while (!isFull() && MIDI.read() && connected) {
    dispatch();
  }
  sendMessages();
}

// returns false if loadMessage failed due to overflow 
boolean dispatch() {
  switch (MIDI.getType()) {
    case midi::NoteOff:
    case midi::NoteOn:
    //case midi::AfterTouchPoly:
    //case midi::ControlChange:
    //case midi::PitchBend:
      return loadMessage(3, MIDI.getType() | MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());

    case midi::ProgramChange:
    //case midi::AfterTouchChannel:
      return loadMessage(2, MIDI.getType() | MIDI.getChannel(), MIDI.getData1(), 0);
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
  sendMessage(3, 0, 0, 0);

  BLE.advertise();
}

boolean isEmpty() {
  return byteOffset == 0;
}

boolean isFull() {
  return byteOffset > BLE_PACKET_SIZE - 4;
}

boolean loadMessage(int msglen, uint8_t status, uint8_t byte1, uint8_t byte2) {
  // Assert BLE_PACKET_SIZE > 4
  if (isFull()) return false;
  boolean wasEmpty = isEmpty();
  uint16_t timestamp = (uint16_t) millis();
  uint8_t msgTs = timestamp;
  msgTs |= 1 << 7;  // set the 7th bit
  if (wasEmpty) {
    uint8_t headTs = timestamp >> 7;
    headTs |= 1 << 7;  // set the 7th bit
    headTs &= ~(1 << 6);  // clear the 6th bit
    midiData[byteOffset++] = headTs;
  }
  if (wasEmpty || lastStatus != status) {
    midiData[byteOffset++] = msgTs;
    midiData[byteOffset++] = status;
  } else if (lastTime != timestamp) {
    midiData[byteOffset++] = msgTs;   
  }
  midiData[byteOffset++] = byte1;
  if (msglen == 3) {
    midiData[byteOffset++] = byte2;
  }
  lastStatus = status;
  lastTime = timestamp;
  return true;
}

boolean sendMessage(int msglen, uint8_t status, uint8_t byte1, uint8_t byte2) {
  return loadMessage(msglen, status, byte1, byte2) && sendMessages();
}

boolean sendMessages() {
  if (isEmpty()) return false;
  midiChar.setValue(midiData, byteOffset);
  byteOffset = 0;
  return true;
}

