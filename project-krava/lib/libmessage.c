#include "libmessage.h"

#define MESSAGE_MAX_SIZE=50

/*
 * Strusture for temperature, coordintate and battery measurements storage
 */
struct Message {
	int temperatures[MESSAGE_MAX_SIZE];
	int temperaturesCount;
	int coordinatesX[MESSAGE_MAX_SIZE];
	int coordinatesY[MESSAGE_MAX_SIZE];
	int coordinatesZ[MESSAGE_MAX_SIZE];
	int coordinatesCount;
	int battery[MESSAGE_MAX_SIZE];
	int batteryCount;
};



