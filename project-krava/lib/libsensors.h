#ifndef __SENSORS_H__
#define __SENSORS_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

float decodeTemperature(int16_t temperature);

void printTemperature(float temperature);

#endif