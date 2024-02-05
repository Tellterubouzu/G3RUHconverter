/*/////////////////////////////////////////////////////////////////////////////
                         About this program
   It receives the information to be downlinked, configures the Ax.25
 protocol, scrambles it in accordance with G3RUH, and transmits it in time
 with the clock sent by the communicator. Use the SPI communication when
 transmit data.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                        			Flowchart
             Receive any transmit byte data sequence (max 256bytes)
            Composed of address, control, PID, and information data
            							Applybit stuffing
          								 NRZI encodeing
          				  Apply 17-bit LFSR scrambling 
          							  Append flags(0x7E)
before and after the frame Send data to the communicator using SPI communication
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                Basic information, precautions and reference
 ####################   Ax.25 UI frame format   ####################
 Frag |Destination adress|Source adress|Control|PID  |Information|FCS   |Flag
 0x7E |   Callsign+SSID  |Callsign+SSID|0x03   |0xF0 |    Data   |CRC   |0x7E
 1Byte|      7Bytes      |    7Bytes   |1Byte  |1Byte|0~256Bytes |2Bytes|1Byte
 >>read more
 >>>https://qiita.com/OzoraKobo/items/e411705f2295d5a6f27d
 
  Considering efficiency including downlink success probability,
110Bytes is a good size for the information field
 ##########################   PIC18LF4620   ######################
 >>readmore
 >>>https://ww1.microchip.com/downloads/aemDocuments/documents/OTH/ProductDocuments/DataSheets/39626e.pdf
 #################Kyutech GS & Satellite callsign#################
 Project   | MITSUBA | YOTSUBA
 Ground    | JG6YBW  | -TBD-
 Satellite | JG6YOL  | -TBD-

 ###########################TXE430FMCW-302A-RU######################
															Mode-setting
PSW : If 5V electric power supply and PSW pin lower than 1V, the communicator
will active. 
					|FMCW0|FMCW1 |mode     |
					|Low  | Low  |Wait mode|
					|Low  |High  |AFSK mode|
					|High | Low  | CW  mode| input CWKEY 
					|High | High |GMSK mode| input TRDAT,clock TRCLK
PinAssign   Information
1 	CWKEY 	Low : Transmit /High : No signal
2		RXS
3		PLOCK
4   TRCLK		4.8kHz clk
5		TRDAT
6		AFIN	 sub carrier of fm signal(1200/2200Hz) input
7		FMPTT	 Low AFSK transmit
8		5V
9		GND
10	FMCW1
12	FMCW0
12	PSW    Only on this pin, communicator will be data receive mode /

/////////////////////////////////////Code////////////////////////////////////*/
// include file list
#include <stdint.h>
#include <stdio.h>
#include <stdlibm.h>
#include <stdlib.h>
// include <18LF4620.h>
// Device configuration
//#invlude <DeviceConfiguration.h>

// Define
//#define Downlink_start 0x00
uint32_t d_shift_register = 0;
uint32_t d_taps[32];
uint32_t d_tap_count;
uint8_t Downlinkdata [256] = {0};
uint8_t Downlinkpacket[500] = {0};
uint16_t packetsize = 0;
// Interrupts

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

void Packetize(uint8_t *data, uint16_t inputsize,uint8_t *packet,uint16_t *outputsize);

// User define Function
void callsign(uint8_t *input, int len, uint8_t *output) {
  output[0] = 0x94;  // J = 4A
  output[1] = 0x8e;  // G = 47
  output[2] = 0x6c;  // 6 = 36
  output[3] = 0xb2;  // Y = 59
  output[4] = 0x84;  // B = 42
  output[5] = 0xae;  // W = 57
  output[6] = 0x60;  // SSID
  output[7] = 0x94;  // J = 4A
  output[8] = 0x8e;  // G = 47
  output[9] = 0x6c;  // 6 = 36
  output[10] = 0xb2; // Y = 59
  output[11] = 0x9E; // O = 4F
  output[12] = 0x98; // L = 4C
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
  int8_t outputbitsnumber = -1;
  uint16_t inputbytes_number;
  int16_t outputbytes_number = 0;

  uint8_t judge = 0;
  uint8_t consecutive_ones = 0;
  
	for(uint8_t i = 0; i <(size*6/5);i++){
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

void Packetize(uint8_t *input, uint16_t inputsize,uint8_t *packet,uint16_t *outputsize) {

  printf("original data     : \r\n");
  uint16_t len = inputsize;
  desplay_data(input, len);

  uint8_t *SIGN;
  SIGN = (uint8_t *)malloc(274);
  callsign(input, len, SIGN);
  len += 16;
  printf("appended call sign PID data     :\r\n");
  desplay_data(SIGN, len);

  uint8_t *CRCdata;
  CRCdata = (uint8_t *)malloc(276);
  crc(SIGN, len, CRCdata);
  free(SIGN);
  len += 2;
  printf("CRC data     : \r\n");
  desplay_data(CRCdata, len);

  uint8_t *stuffed;
  stuffed = (uint8_t *)malloc(330);
  bitstuffing(CRCdata, len, stuffed, &packetsize);
  //printf("len =%lu\r\n", len);
  printf("Bitstuffing :\r\n");
  len = packetsize;
  //printf("len =%lu\r\n", len);
  desplay_data(stuffed, len);

  nrzi_encode(stuffed, stuffed, len);
  //nrzi_encode(CRCdata,CRCdata,len);
  printf("nrzi encoded      : \r\n");
  //desplay_data(CRCdata,len);
  desplay_data(stuffed, len);

  g3ruh_init(0x02080UL);
  g3ruh_scrambler(stuffed, stuffed, len);
  //g3ruh_scrambler(CRCdata,CRCdata,len);
  printf("g3ruh scrambled   : \r\n");
  desplay_data(stuffed, len);
  //desplay_data(CRCdata,len);

  uint8_t Appendedflag[276];
  append_flag(stuffed, len, Appendedflag);
  //append_flag(CRCdata,len,Appendedflag);
  printf("Appendflag :\r\n");
  len += 2;
  desplay_data(Appendedflag, len);

  for (uint16_t i = 0; i < len; i++) {
    //spi_write(Appendedflag[i]);
    Downlinkpacket[i] = Appendedflag[i];
  }
  packetsize = len;
  printf("packetize end\r\n");
  //enable_interrupts(INT_SSP);
}
// void main
/*
int main(void) {
  // bitstuff   NRZI ͋t

  uint8_t uplinkdata[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                          0x06, 0x07, 0x08, 0x09, 0xA0, 0xA1};
  printf("packetsize is %02X , adrs: %p\r\n", packetsize, &packetsize);
  packetsize = 12;
  Packetize(uplinkdata, packetsize, Downlinkpacket, packetsize);

  printf("packetsize is %d\r\n", packetsize);
  printf("in main function\r\n");

  for (uint16_t i = 0; i < packetsize; i++) {
  	spi_write(Downlinkpacket[i]);
    //printf("%02X ", Downlinkpacket[i]);
    //if (i % 16 == 15) {
      //printf("\n\n");
    }
  }
  printf("\n\n");

  return 0;
}
*/


