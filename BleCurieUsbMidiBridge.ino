/* Bridge from USB MIDI to CurieBLE
 *
 * Components:
 *
 * 1. Hobbytronics USB MIDI Host (5v)
 * 2. Bi-directional logic level shifter
 * 3. Arduino 101 (3.3v)
 *
 * Connections:
 *
 * Hobbytronics TX -> HV-LV -> Arduino bit 0 
 * Hobbytronics RX <- HV-LV <- Arduino bit 1 
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
uint32_t lastTime;

void setup() {
  connected = false;

  pinMode(LED_PIN, OUTPUT);
  displayConnectionState();

  MIDI.setHandleNoteOn(handleMessage3);
  MIDI.setHandleNoteOff(handleMessage3);
  //MIDI.setHandleControlChange(handleMessage3);
  MIDI.begin(MIDI_CHANNEL_OMNI);

  setupBle();
}

void loop() {
  BLE.poll();
  connected = !!BLE.central();
  displayConnectionState();
  if (connected) {
    while (MIDI.read());
    sendMessages();
  }
}

//void testNotes() {
//  noteOn(0, 60, 127);
//  midiChar.setValue(midiData, 5);
//  delay(200);
//  noteOff(0, 60);
//  midiChar.setValue(midiData, 5);
//  delay(400);
//}

// MIDI handler funtions. See http://arduinomidilib.fortyseveneffects.com

void handleMessage3(byte channel, byte number, byte value) {
  uint8_t status = MIDI.getType() | channel;
  uint8_t byte1 = number & 0x7F;
  uint8_t byte2 = value & 0x7F;
  if (!loadMessage(status, byte1, byte2)) {
    sendMessages() && loadMessage(status, byte1, byte2);
  }
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
  sendMessage(0, 0, 0);

  BLE.advertise();
}

boolean loadMessage(uint8_t status, uint8_t byte1, uint8_t byte2) {
  // Assert BLE_PACKET_SIZE > 4
  if (byteOffset > BLE_PACKET_SIZE - 4) return false;
  uint32_t timestamp = millis();
  boolean empty = byteOffset == 0;
  if (empty) {
    uint8_t headTs = timestamp >> 7;
    headTs |= 1 << 7;  // set the 7th bit
    headTs &= ~(1 << 6);  // clear the 6th bit
    midiData[byteOffset++] = headTs;
  }
  if (empty || lastStatus != status || lastTime != timestamp) {
    uint8_t msgTs = timestamp;
    msgTs |= 1 << 7;  // set the 7th bit
    midiData[byteOffset++] = msgTs;
    midiData[byteOffset++] = status;
    midiData[byteOffset++] = byte1;
    midiData[byteOffset++] = byte2;
    lastStatus = status;
    lastTime = timestamp;
  } else {
    midiData[byteOffset++] = byte1;
    midiData[byteOffset++] = byte2;
  }
  return true;
}

boolean sendMessage(uint8_t status, uint8_t byte1, uint8_t byte2) {
  return loadMessage(status, byte1, byte2) && sendMessages();
}

boolean sendMessages() {
  if (byteOffset != 0) {
    midiChar.setValue(midiData, byteOffset);
    byteOffset = 0;
    return true;
  }
  return false;
}
