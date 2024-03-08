/*/////////////////////////////////////////////////////////////////////////////
\\\About this program
   It receives the information to be downlinked, configures the Ax.25
 protocol, scrambles it in accordance with G3RUH, and transmits it in time
 with the clock sent by the communicator. Use the SPI communication when
 transmit data.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
\\\Flowchart
****encode****
Receive any transmit byte data sequence (max 256bytes)
Composed of address, control, PID, and information data
Applybitstuffing
NRZI encodeing
Apply 17-bit LFSR scrambling
Append flags(0x7E) before and after the frame
Send data to the communicator using SPI communication
****decode****
Remove flag
Apply descrambling
Apply NRZI decode
debitstuffing
confirm CRC
confirm Address,PID,control
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 How to use this header file
[1] Ax.25packetize
As shown in the example in the main function, a function named Packetize is used
with three arguments: an array containing the data to be packetized, an array
for output, and the size of the packet to be output. (Prepare an 8-bit array
containing the data to be downlinked. The first byte of the data is the address
and the second byte is the array size.)

[2] Packet encode
This is also shown in the main function, but with four arguments

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                Basic information, precautions and reference
 ####################   Ax.25 UI frame format   ####################
 Frag |Destination adress|Source adress|Control|PID  |Information|FCS   |Flag
 0x7E |   Callsign+SSID  |Callsign+SSID|0x03   |0xF0 |    Data   |CRC   |0x7E
 1Byte|      7Bytes      |    7Bytes   |1Byte  |1Byte|0~256Bytes |2Bytes|1Byte
 
/////////////////////////////////////Code////////////////////////////////////*/
// include file list
#include <stdint.h>
#include <stdio.h>

#include <stdlib.h>
// Define
//#define Downlink_start 0x00

uint32_t d_shift_register = 0;
uint32_t d_taps[32];
uint32_t d_tap_count;

uint8_t Downlinkpacket[500] = {0};
uint16_t packetsize = 0;

// Prototype declaration
void callsign(uint8_t *input, int len, uint8_t *output);
void crc(uint8_t *input, int len, uint8_t *output);
void bitstuffing(uint8_t *data, uint16_t size, uint8_t *output,
                 uint16_t *output_size);
void g3ruh_init(uint32_t tap_mask);
void g3ruh_scrambler(uint8_t *unscrambled, uint8_t *scrambled, uint16_t len);
void nrzi_encode(uint8_t *buf, uint8_t *output, uint16_t len);
void desplay_data(uint8_t *data, uint16_t len);
void append_flag(uint8_t *data, uint16_t len, uint8_t *output);

void Packetize(uint8_t *data, uint8_t *output, uint16_t *outputsize);

void byte_to_bit(uint8_t *data, uint16_t len);

// User define Function
void callsign(uint8_t *input, int len, uint8_t *output) {
  output[0] = 0x94;  // J = 4A
  output[1] = 0x94;  //
  output[2] = 0x94;  // 
  output[3] = 0x94;  // 
  output[4] = 0x94;  // 
  output[5] = 0x94;  // 
  output[6] = 0x60;  // SSID
  output[7] = 0x94;  // J = 4A
  output[8] = 0x94;  // 
  output[9] = 0x94;  // 
  output[10] = 0x94; // 
  output[11] = 0x94; // 
  output[12] = 0x94; // 
  output[13] = 0xe1; // SSID
  output[14] = 0x03; // control (3)
  output[15] = 0xf0; // PID
  for (int i = 0; i <= len; i++) {
    output[16 + i] = input[i];
  }
}

void crc(uint8_t *input, int len, uint8_t *output) {
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < len; i++) {
    crc ^= (uint16_t)input[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  output[len] = crc & 0xFF;
  output[len + 1] = (crc >> 8) & 0xFF;
  for (int k = 0; k < len; k++) {
    output[k] = input[k];
  }
}

void bitstuffing(uint8_t *data, uint16_t size, uint8_t *output,
                 uint16_t *outputsize) {
  uint8_t inputbitsnumber;
  static int8_t outputbitsnumber = -1;
  uint16_t inputbytes_number;
  static int16_t outputbytes_number = 0;

  uint8_t judge = 0;
  uint8_t consecutive_ones = 0;

  for (uint8_t i = 0; i < (size * 6 / 5); i++) {
    output[i] = 0;
  }

  for (inputbytes_number = 0; inputbytes_number <= size; inputbytes_number++) {
    if (inputbytes_number == size) {
      judge = 0;
    } else {
      judge = data[inputbytes_number];
    }

    for (inputbitsnumber = 0; inputbitsnumber < 8; inputbitsnumber++) {
      outputbitsnumber += 1;
      if ((judge >> (7 - inputbitsnumber)) & 0x01) {
        consecutive_ones += 1;
      } else {
        consecutive_ones = 0;
      }
      if (consecutive_ones == 6) {
        outputbitsnumber += 1;
        consecutive_ones = 0;
      }
      if (outputbitsnumber >= 8) {
        outputbitsnumber -= 8;
        outputbytes_number += 1;
      }

      output[outputbytes_number] |= ((judge >> (7 - inputbitsnumber)) & 0x01)
                                    << (7 - outputbitsnumber);
    }
  }

  *outputsize = outputbytes_number;
}

void g3ruh_init(uint32_t tap_mask) {
  uint8_t i;
  d_tap_count = 0;

  for (i = 0; i < 32; i++) {
    if ((tap_mask & 0x01) == 1) {
      d_taps[d_tap_count] = i;
      d_tap_count++;
    }
    tap_mask = tap_mask >> 1;
  }

  d_shift_register = 0;
}

void g3ruh_scrambler(uint8_t *unscrambled, uint8_t *scrambled, uint16_t len) {
  uint8_t unscrambled_bit;
  uint8_t scrambled_bit;
  uint32_t tap_bit;
  uint16_t i, j, t;

  for (i = 0; i < len; i++) {
    for (j = 0; j < 8; j++) {
      unscrambled_bit = unscrambled[i] >> j & 0x01;
      d_shift_register <<= 1;

      scrambled_bit = unscrambled_bit;
      for (t = 0; t < d_tap_count; t++) {
        tap_bit = (d_shift_register >> d_taps[t]) & 0x01;
        scrambled_bit = scrambled_bit ^ tap_bit;
      }

      d_shift_register |= scrambled_bit;

      if (scrambled_bit)
        scrambled[i] |= (1 << j);
      else
        scrambled[i] &= ~(1 << j);
    }
  }
}

void nrzi_encode(uint8_t *buf, uint8_t *output, uint16_t len) {
  uint8_t prev_nrzi_bit = 0;
  uint8_t nrz_bit;
  uint8_t nrzi_bit;
  uint16_t i, j;

  for (i = 0; i < len; i++) {
    for (j = 0; j < 8; j++) {
      nrz_bit = buf[i] >> j & 0x01;

      if (nrz_bit == 0)
        nrzi_bit = prev_nrzi_bit ^ 1;
      else
        nrzi_bit = prev_nrzi_bit;

      if (nrzi_bit)
        output[i] |= (1 << j);
      else
        output[i] &= ~(1 << j);

      prev_nrzi_bit = nrzi_bit;
    }
  }
}

void desplay_data(uint8_t *data, uint16_t len) {
  for (uint16_t i = 0; i < len; i++) {
    printf("%02X ", data[i]);
    if (i % 16 == 15) {
      printf("\r\n\n");
    }
  }
  printf("\r\n\n");
}

void append_flag(uint8_t *input, uint16_t len, uint8_t *output) {
  output[0] = 0x7E;
  for (int i = 0; i < len; i++) {
    output[i + 1] = input[i];
  }
  output[len + 1] = 0x7E;
}

void Packetize(uint8_t *input, uint8_t *output, uint16_t *outputsize) {

  printf("original data     : \r\n");
  uint16_t len = input[1];
  byte_to_bit(input, len);

  uint8_t *SIGN;
  SIGN = (uint8_t *)malloc(274);
  callsign(input, len, SIGN);
  len += 16;
  printf("appended call sign PID data     :\r\n");
  byte_to_bit(SIGN, len);

  uint8_t *CRCdata;
  CRCdata = (uint8_t *)malloc(276);
  crc(SIGN, len, CRCdata);
  free(SIGN);
  len += 2;
  printf("CRC data     : \r\n");
  byte_to_bit(CRCdata, len);

  uint8_t *stuffed;
  stuffed = (uint8_t *)malloc(330);
  bitstuffing(CRCdata, len, stuffed, &packetsize);
  printf("Bitstuffing :\r\n");
  len = packetsize;
  byte_to_bit(stuffed, len);

  nrzi_encode(stuffed, stuffed, len);
  printf("nrzi encoded      : \r\n");
  byte_to_bit(stuffed, len);

  uint32_t d_shift_register = 0;
  uint32_t d_taps[32];
  uint32_t d_tap_count;
  g3ruh_init(0x02080UL);
  g3ruh_scrambler(stuffed, stuffed, len);
  printf("g3ruh scrambled   : \r\n");
  byte_to_bit(stuffed, len);
  uint8_t Appendedflag[276];
  append_flag(stuffed, len, Appendedflag);
  printf("Appendflag :\r\n");
  len += 2;
  byte_to_bit(Appendedflag, len);

  for (uint16_t i = 0; i < len; i++) {
    Downlinkpacket[i] = Appendedflag[i];
  }
  *outputsize = len;
}

void byte_to_bit(uint8_t *data, uint16_t len) {
  uint8_t bit = 0;
  for (uint16_t i = 0; i < len; i++) {
    printf("%02X ", data[i]);
    for (uint8_t bits = 0; bits < 8; bits++) {
      bit = ((data[i] >> (7 - bits)) & 0x01);
      printf(" %d ", bit);
      if (bits == 7) {
        printf("\r\n");
      }
    }
  }
}
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Anything written below this is for decryption
*/

/// prototype declaration
void decode(uint8_t *data, uint16_t datasize, uint8_t *output,
            uint16_t *outputsize);
void debitstuffing(uint8_t *input, uint16_t inputsize, uint8_t *output,
                   uint16_t *outputsize);
void nrzi_decode(uint8_t *buf, uint8_t *output, uint16_t len);
void G3RUH_descrambler(uint8_t *scrambled, uint8_t *unscrambled, uint16_t len);
void g3ruh_init_de(uint32_t tap_mask);
void remove_flag(uint8_t *input, uint16_t inputsize, uint8_t *output);

void confirmCRC(uint8_t *daa, uint16_t datasize);

void confirmcallsign(uint8_t *input, uint16_t len, uint8_t *output);
///*User defined function for decryption*/

void g3ruh_init_de(uint32_t tap_mask) {
  uint8_t i;
  d_tap_count = 0;

  for (i = 0; i < 32; i++) {
    if ((tap_mask & 0x01) == 1) {
      d_taps[d_tap_count] = i;
      d_tap_count++;
    }
    tap_mask = tap_mask >> 1;
  }

  d_shift_register = 0;
}
void G3RUH_descrambler(uint8_t *scrambled, uint8_t *unscrambled, uint16_t len) {
  uint8_t unscrambled_bit;
  uint8_t scrambled_bit;
  uint32_t tap_bit;
  uint16_t i, j, t;

  for (i = 0; i < len; i++) {
    for (j = 0; j < 8; j++) {
      scrambled_bit = scrambled[i] >> j & 0x01;
      d_shift_register <<= 1;

      unscrambled_bit = scrambled_bit;
      for (t = 0; t < d_tap_count; t++) {
        tap_bit = (d_shift_register >> d_taps[t]) & 0x01;
        unscrambled_bit = unscrambled_bit ^ tap_bit;
      }

      d_shift_register |= scrambled_bit;

      if (unscrambled_bit)
        unscrambled[i] |= (1 << j);
      else
        unscrambled[i] &= ~(1 << j);
    }
  }
}

void nrzi_decode(uint8_t *buf, uint8_t *output, uint16_t len) {
  uint8_t prev_nrzi_bit = 0;
  uint8_t nrz_bit;
  uint8_t nrzi_bit;
  uint16_t i, j;

  for (i = 0; i < len; i++) {
    for (j = 0; j < 8; j++) {
      nrzi_bit = buf[i] >> j & 0x01;

      if (nrzi_bit != prev_nrzi_bit)
        nrz_bit = 0;
      else
        nrz_bit = 1;

      if (nrz_bit)
        output[i] |= (1 << j);
      else
        output[i] &= ~(1 << j);

      prev_nrzi_bit = nrzi_bit;
    }
  }
}

void debitstuffing(uint8_t *input, uint16_t inputsize, uint8_t *output,
                   uint16_t *outputsize) {
  uint16_t outbytes = 0, inbytes = 0, bits = -1;
  uint8_t inbits = 0, oubits = -1, extracted_bit = 0, consecutive_ones = 0,
          skip = 0;
  for (uint8_t i = 0; i < (inputsize * 6 / 5); i++) {
    output[i] = 0;
  }

  for (inbytes = 0; inbytes < inputsize; inbytes++) {
    for (inbits = 0; inbits < 8; inbits++) {
      extracted_bit = ((input[inbytes] >> (7 - inbits)) & 0x01);
      if (extracted_bit == 1) {
        consecutive_ones += 1;
      } else {
        consecutive_ones = 0;
      }

      if (skip) {
        bits -= 1;
        skip = 0;
        extracted_bit = 1;
      } else if (consecutive_ones == 5) {
        skip = 1;
      }

      bits += 1;

      outbytes = bits / 8;
      oubits = bits % 8;
      output[outbytes] |= extracted_bit << (7 - oubits);
    }
  }
  *outputsize = outbytes;
}

void remove_flag(uint8_t *input, uint16_t inputsize, uint8_t *output) {

  for (uint16_t i = 1; i < inputsize; i++) {
    output[i - 1] = input[i];
  }
}

void confirmCRC(uint8_t *data, uint16_t datasize) {
  uint8_t CRC[2];
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < datasize - 2; i++) {
    crc ^= (uint16_t)data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  CRC[0] = crc & 0xFF;
  CRC[1] = (crc >> 8) & 0xFF;

  if (CRC[0] == data[datasize - 2] && CRC[1] == data[datasize - 1]) {
    printf(" Frame check sequence confirmed\r\n");
  } else {
    printf(" Input data maybe broken\r\n");
  }
}

void confirmcallsign(uint8_t *input, uint16_t len, uint8_t *output) {
  if (input[0] == 0x94 && input[1] == 0x8E && input[2] == 0x6C &&
          input[3] == 0xB2 && input[4] == 0x84 && input[5] == 0xAE &&
          input[6] == 0x60,
      input[7] == 0x94 && input[8] == 0x8E && input[9] == 0x6C &&
          input[10] == 0xB2 && input[11] == 0x9E && input[12] == 0x98 &&
          input[13] == 0xE1 && input[14] == 0x03 && input[15] == 0xF0) {
    printf("This signal is came from YOTSUBA\r\n");
    for (uint8_t i = 0; i < len - 16; i++) {
      output[i] = input[i + 16];
    }
  } else {
    printf("Address,PID,Control field is error");
    for (uint8_t j = 0; j < len; j++) {
      output[j] = input[j];
    }
  }
}

void decode(uint8_t *data, uint16_t datasize, uint8_t *output,
            uint16_t *outputsize) {
  /* 3rd number of the input array is */
  /*
  flag PID cmd datasize information CRC flag
*/
  remove_flag(data, datasize, data);
  datasize -= 2;
  printf("remove_flag : \r\n");
  byte_to_bit(data, datasize);

  uint32_t d_shift_register = 0;
  uint32_t d_taps[32];
  uint32_t d_tap_count;
  g3ruh_init_de(0x2080UL);
  G3RUH_descrambler(data, data, datasize);
  printf("descramble : \r\n");
  byte_to_bit(data, datasize);

  printf("nrzidecode\r\n");
  nrzi_decode(data, data, datasize);
  byte_to_bit(data, datasize);

  uint8_t *destuffed;
  destuffed = (uint8_t *)malloc(330);
  uint16_t destuffedsize;
  debitstuffing(data, datasize, destuffed, &destuffedsize);
  printf("destuffedsize = %d\r\n", destuffedsize);
  printf("destuffed : \r\n");
  byte_to_bit(destuffed, destuffedsize);
  confirmCRC(destuffed, destuffedsize);
  destuffedsize -= 2;
  byte_to_bit(destuffed, destuffedsize);
  confirmcallsign(destuffed, destuffedsize, destuffed);
  destuffedsize -= 16;
  byte_to_bit(destuffed, destuffedsize);
  for (uint8_t i = 0; i < destuffedsize; i++) {
    output[i] = destuffed[i];
  }

  *outputsize = destuffedsize;
}

// void main
int main(void) {

  uint8_t uplinkdata[] = {0x55, 0x07, 0xB0, 0x6F, 0x00, 0x7E, 0xFF};

  Packetize(uplinkdata, Downlinkpacket, &packetsize);

  printf("in main function packetize data is\r\n");

  desplay_data(Downlinkpacket, packetsize);

  uint8_t *source;
  source = (uint8_t *)malloc(330);
  uint16_t sourcesize;
  decode(Downlinkpacket, packetsize, source, &sourcesize);
  printf("in main function decoded data is \r\n");
  byte_to_bit(source, sourcesize);

  printf("\n\n");

  return 0;
}
