#ifndef __SENSORS_H__
  #include "libsensors.h"
#endif 

// Returns temp ranging from -127 to 127 degrees Celisus
int8_t decodeTemperature(int16_t raw){
  
  /**
  * Decode raw temperature to float
  */

  int16_t  tempint;
  // uint16_t tempfrac;
  uint16_t absraw;    
  int16_t  sign = 1;
  // char     minus = ' ';  
  // float temperature;
  
  absraw = raw;
  if (raw < 0) { // Perform 2C's if sensor returned negative data
    absraw = (raw ^ 0xFFFF) + 1;
    sign = -1;
  }
  if ((absraw >> 8) > 127) {
      absraw = 127;
    }
  tempint  = (absraw >> 8) * sign;
  // tempfrac = ((absraw>>4) % 16) * 625; // Info in 1/10000 of degree
  // minus = ((tempint == 0) & (sign == -1)) ? '-'  : ' ' ;

  // temperature = tempint + tempfrac/10000;
  // printf ("Temperature = %d.%d\n", tempint, tempfrac);

  return ((int8_t)tempint);

}

void printTemperature(int8_t temperature){

  /**
  * Print raw temperature value to string
  */  

  printf ("Temperature = %d\n", temperature);

}

float myFloor(float x){
  if(x >= 0.0f) {
    return (float)((int)x);
  } else {
    return (float)((int)x - 1);
  }
}

uint8_t decodeBattery(uint16_t batraw) {
  int8_t tmp = ((batraw * 5000 / 4096) - BAT_EMPT_VAL) * 100 / (BAT_FUL_VAL - BAT_EMPT_VAL);
  if (tmp > 100) {
      tmp = 100;
  } else if (tmp < 0) {
      tmp = 0;
  }
  return ((uint8_t) tmp);
}