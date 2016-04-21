#ifndef __SENSORS_H__
#define __SENSORS_H__

#include <stdint.h>
#include <stdio.h>


static int16_t readTemperature();

static float decodeTemperature(int16_t temperature);

static void printTemperature(float temperature);

#endif