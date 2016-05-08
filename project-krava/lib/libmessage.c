#ifndef __MESSAGE_H__
  #include "libmessage.h"
#endif

#include <stdio.h>

#define MESSAGE_MAX_SIZE 50

/*
 * Strusture for temperature, coordintate and battery measurements storage
 */
struct Message {
	int temps[MESSAGE_MAX_SIZE];
	int tempsCount;
	int accelerations[MESSAGE_MAX_SIZE];
	int accelerationsCount;
	int batteries[MESSAGE_MAX_SIZE];
	int batteriesCount;
};

/*
 * Helper function for adding temperature into struct
 */
void addTemperature (struct Message m, int temperature) {
	int tempC = m.tempsCount;
	// if the temperatures stack is full ditch the last measurement to make room for new one
	if (tempC == MESSAGE_MAX_SIZE) {
		int i;
		for (i = 1; i < MESSAGE_MAX_SIZE; i = i + 1) {
			m.temps[i - 1] = m.temps[i];
		}
		tempC = MESSAGE_MAX_SIZE - 1;
	}
	m.temps[tempC] = temperature;
	m.tempsCount = tempC + 1;
}

/*
 * Helper function for adding acceleration into struct
 */
void addAcceleration (struct Message m, int acceleration) {
	int accC = m.accelerationsCount;
	// if the acceleration stack is full ditch the last measurement to make room for new one
	if (accC == MESSAGE_MAX_SIZE) {
		int i;
		for (i = 1; i < MESSAGE_MAX_SIZE; i = i + 1) {
			m.accelerations[i - 1] = m.accelerations[i];
		}
		accC = MESSAGE_MAX_SIZE - 1;
	}
	m.accelerations[accC] = acceleration;
	m.accelerationsCount = accC + 1;
}

/*
 * Helper function for adding battery status into struct
 */
void addBattery (struct Message m, int battery) {
	int batteryC = m.batteriesCount;
	// if the battery status stack is full ditch the last measurement to make room for new one
	if (batteryC == MESSAGE_MAX_SIZE) {
		int i;
		for (i = 1; i < MESSAGE_MAX_SIZE; i = i + 1) {
			m.batteries[i - 1] = m.batteries[i];
		}
		batteryC = MESSAGE_MAX_SIZE - 1;
	}
	m.batteries[batteryC] = battery;
	m.batteriesCount = batteryC + 1;
}

/*
 * Encode message struct into int array to be sent
 */
int * encodeData(struct Message m) {
	static int buffer[3 * MESSAGE_MAX_SIZE];

	int i;
	for (i = 0; i < m.tempsCount; i = i + 1) {
		buffer[i] = m.temps[i];
	}
	for (i = 0; i < m.accelerationsCount; i = i + 1) {
		buffer[i + m.tempsCount] = m.accelerations[i];
	}
	for (i = 0; i < m.batteriesCount; i = i + 1) {
		buffer[i + m.tempsCount + m.accelerationsCount] = m.batteries[i];
	}
	return buffer;
}

/*
 * Returns encoded data size (size of int array)
 */
int getEncodeDataSize(struct Message m) {
	int size = m.tempsCount + m.accelerationsCount + m.batteriesCount;
	return size;
}

/*
 * Prints data from Message struct
 */
void printMessage(struct Message m) {
	//char buffer [m.tempsCount * 1 + m.accelerationsCount * 3 + m.batteriesCount * 1];
	//int bufferSize = 0;
	//bufferSize = sprintf(buffer, "%d plus %d is %d", a, b, a+b);
	printf("Message struct contains: \n %2d temperature measurements, \n %2d acceleration measurements, \n %2d battery measurements ", 
		m.tempsCount, m.accelerationsCount, m.batteriesCount);
}

/*
 * Decodes array of ints into Message struct
 */
void decode(char * message) {
	// TODO: to be implemented
	printf("To be implemented");
}



