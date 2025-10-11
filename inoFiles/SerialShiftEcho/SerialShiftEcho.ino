// ====================== SerialShiftEcho.ino ======================
/*
 * Receives one COBS-encoded byte from Qt,
 * The left-shifted result is returned COBS-encoded.
 */

#include <Arduino.h>

// --- COBS Decode ---
int COBS_Decode(const uint8_t *input, int length, uint8_t *output)
{
  int read_index = 0, write_index = 0;
  while (read_index < length)
  {
    uint8_t code = input[read_index];
    if (code == 0 || read_index + code > length + 1) return -1; // invalid
    read_index++;
    for (uint8_t i = 1; i < code; i++)
    {
      output[write_index++] = input[read_index++];
    }
    if (code != 0xFF && read_index < length)
    {
      output[write_index++] = 0;
    }
  }
  return write_index;
}

// --- COBS Encode ---
int COBS_Encode(const uint8_t *input, int length, uint8_t *output)
{
  int read_index = 0, write_index = 1;
  int code_index = 0;
  uint8_t code = 1;

  while (read_index < length)
  {
    if (input[read_index] == 0)
    {
      output[code_index] = code;
      code = 1;
      code_index = write_index++;
      read_index++;
    }
    else
    {
      output[write_index++] = input[read_index++];
      code++;
      if (code == 0xFF)
      {
        output[code_index] = code;
        code = 1;
        code_index = write_index++;
      }
    }
  }
  output[code_index] = code;
  output[write_index++] = 0x00; // terminator
  return write_index;
}

void setup()
{
  Serial.begin(115200);
}

void loop()
{
  static uint8_t buffer[16];
  static uint8_t decoded[8];

  static int count = 0;
  while (Serial.available())
  {
    uint8_t b = Serial.read();
    buffer[count++] = b;

    if (b == 0x00) // frame end
    {
      int decodedLen = COBS_Decode(buffer, count - 1, decoded);
      if (decodedLen == 1)
      {
        uint8_t val = decoded[0];
        uint8_t shifted = val << 1;

        uint8_t encoded[8];
        int encLen = COBS_Encode(&shifted, 1, encoded);
        Serial.write(encoded, encLen);
      }
      count = 0;
    }
  }
}
