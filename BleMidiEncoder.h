/* BleMidiEncoder rev. 1 */

#ifndef BLE_MIDI_ENCODER_H
#define BLE_MIDI_ENCODER_H

#define BLE_MIDI_PACKET_SIZE 20

class BleMidiEncoder {

public:
  boolean isEmpty() {
    return byteOffset == 0;
  }
  
  boolean isFull() {
    return byteOffset > BLE_MIDI_PACKET_SIZE - 4;
  }

  boolean appendMessage(uint8_t status, uint8_t byte1, uint8_t byte2) {
    return loadMessage(3, status, byte1, byte2);
  }
  
  boolean appendMessage(uint8_t status, uint8_t byte1) {
    return loadMessage(2, status, byte1, 0);
  }
  
  boolean sendMessage(uint8_t status, uint8_t byte1, uint8_t byte2) {
    return appendMessage(status, byte1, byte2) && sendMessages();
  }

  boolean sendMessage(uint8_t status, uint8_t byte1) {
    return appendMessage(status, byte1) && sendMessages();
  }

  boolean sendMessages() {
    if (isEmpty() || !setValue(midiData, byteOffset)) {
      return false;
    }
    byteOffset = 0;
    return true;
  }

  virtual ~BleMidiEncoder() {}

protected:
  virtual bool setValue(const unsigned char[] /*value*/, unsigned char /*length*/) = 0;

private:
  boolean loadMessage(int msglen, uint8_t status, uint8_t byte1, uint8_t byte2) {
    // Assert BLE_MIDI_PACKET_SIZE > 4
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

  uint8_t midiData[BLE_MIDI_PACKET_SIZE];
  int byteOffset = 0;
  uint8_t lastStatus;
  uint16_t lastTime;
};

#endif

