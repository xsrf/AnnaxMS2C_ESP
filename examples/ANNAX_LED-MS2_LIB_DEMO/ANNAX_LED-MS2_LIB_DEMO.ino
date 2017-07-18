/*
*/
 
#include <AnnaxMS2C_ESP.h>
uint8_t* buffer;

void setup() { 
  AnnaxMS2_SetRowInterval(500);
  AnnaxMS2_Init();
  AnnaxMS2_SetBitOrder(MSBFIRST);
}


void loop() {
  buffer = AnnaxMS2_GetBackBuffer();
  memset(buffer,0x00,36*18);
  buffer = AnnaxMS2_GetFrontBuffer();
  memset(buffer,0x00,36*18);
  buffer[0] = B11111111;
  buffer[2] = B00001111;
  buffer[4] = B00000011;
  AnnaxMS2_SetFrameBufferLayout(0);
  for(uint8_t i=0; i<1; i++) {
    delay(1000);
    AnnaxMS2_SetFrameBufferLayout(1);
    delay(1000);
    AnnaxMS2_SetFrameBufferLayout(0);
  }

  buffer = AnnaxMS2_GetBackBuffer();
  memset(buffer,B01100110,36*18);
  buffer = AnnaxMS2_GetFrontBuffer();
  memset(buffer,B00000110,36*18);

  AnnaxMS2_SetUseGreyscale(1);
  delay(3000);
  AnnaxMS2_WaitSync();
  AnnaxMS2_SetUseGreyscale(0);
  
  memset(buffer,0x00,36*18);
  memset(buffer,0xFF,4*18);

  for(uint8_t i=0; i<18; i++) {
    AnnaxMS2_SetBrightPulseDelay(i);
    buffer[18*9] = i;
    delay(100);
  }

  for(uint8_t i=18; i>0; i--) {
    AnnaxMS2_SetBrightPulseDelay(i);
    buffer[18*9] = i;
    delay(100);
  }

  AnnaxMS2_SetBrightPulseDelay(12);
  buffer[18*9] = 0;

  memset(buffer,0x00,36*18);
  for(uint16_t i=0; i<36*18; i++) {
    buffer[i] = i&0xFF;
    delay(10);
  }
  AnnaxMS2_SetFrameBufferLayout(1);
  memset(buffer,0x00,36*18);
  for(uint16_t i=0; i<36*18; i++) {
    buffer[i] = i&0xFF;
    delay(10);
  }
  memset(buffer,0x00,36*18);
  AnnaxMS2_SetFrameBufferLayout(0);

  delay(500);
  
}


