#ifndef __SENSORS_H__
#define __SENSORS_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "contiki.h"
#include "dev/i2cmaster.h"  // Include IC driver
#include "dev/tmp102.h"     // Include sensor driver


int16_t readTemperature();

float decodeTemperature(int16_t temperature);

void printTemperature(float temperature);

#endif