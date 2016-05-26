#ifndef __SENSORS_H__
#define __SENSORS_H__

#define BAT_EMPT_VAL 2200
#define BAT_FUL_VAL 3050

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int8_t decodeTemperature(int16_t temperature);

void printTemperature(int8_t temperature);

float myFloor(float x);

uint8_t decodeBattery(uint16_t batraw);

#endif