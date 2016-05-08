#ifndef __SENSORS_H__
#define __SENSORS_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int16_t decodeTemperature(int16_t temperature);

void printTemperature(int16_t temperature);

float myFloor(float x);

#endif