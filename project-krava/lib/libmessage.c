#ifndef __MESSAGE_H__
  #include "libmessage.h"
#endif

#include <stdio.h>
#include <stdlib.h>

//#define MESSAGE_MAX_SIZE 50
//
///*
// * Strusture for temperature, coordintate and battery measurements storage
// */
//struct Message {
//	int temps[MESSAGE_MAX_SIZE];
//	uint tempsCount:8;
//	int accelerations[MESSAGE_MAX_SIZE];
//	uint accelerationsCount:8;
//	int batteries[MESSAGE_MAX_SIZE];
//	uint batteriesCount:8;
//};

/*
 * Sets random number to message structure - we can assume the number will be unique in our system
 */
struct Message setID (struct Message m) {
	int r = rand();
	m.id = r;
	return m;
}


/*
 * Helper function for adding temperature into struct
 */
struct Message addTemperature (struct Message m, int temperature) {
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

	return m;
}

/*
 * Helper function for adding acceleration into struct
 */
struct Message addAcceleration (struct Message m, int acceleration) {
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

	return m;
}

/*
 * Helper function for adding battery status into struct
 */
struct Message addBattery (struct Message m, int battery) {
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

	return m;
}


/*
 * Reset struct Message and set values to empy
 */
struct Message resetMessage(struct Message m){
	m.tempsCount = 0;
	m.accelerationsCount = 0;
	m.batteriesCount = 0;

	return m;
}
/*
 * Encode message struct into int array to be sent
 */
int * encodeData(struct Message m) {
	static int buffer[3 + (3 * MESSAGE_MAX_SIZE)];

	int i;
	int j;
	buffer[0] = m.tempsCount;
	buffer[1] = m.accelerationsCount;
	buffer[2] = m.batteriesCount;
	j = 3;
	for (i = 0; i < m.tempsCount; i = i + 1) {
		buffer[j] = m.temps[i];
		j += 1;
	}
	for (i = 0; i < m.accelerationsCount; i = i + 1) {
		buffer[j] = m.accelerations[i];
		j += 1;
	}
	for (i = 0; i < m.batteriesCount; i = i + 1) {
		buffer[j] = m.batteries[i];
		j += 1;
	}

	return buffer;
}

/*
 * Returns encoded data size (size of int array)
 */
int getEncodeDataSize(struct Message m) {
	int size = 3 + m.tempsCount + m.accelerationsCount + m.batteriesCount;
	return size;
}

/*
 * Prints data from Message struct
 */
void printMessage(struct Message m) {
	//char buffer [m.tempsCount * 1 + m.accelerationsCount * 3 + m.batteriesCount * 1];
	//int bufferSize = 0;
	//bufferSize = sprintf(buffer, "%d plus %d is %d", a, b, a+b);
	printf("Message struct contains: \n %2d temperature measurements, \n %2d acceleration measurements, \n %2d battery measurements \n", 
		m.tempsCount, m.accelerationsCount, m.batteriesCount);
	printf("Temperature measurements:\n");
	int i;
    for (i = 0; i < m.tempsCount; i = i + 1) {
		printf("%d ", m.temps[i]);
	}
	printf("\n");
	printf("Acceleration measurements:\n");
	for (i = 0; i < m.accelerationsCount; i = i + 1) {
		printf("%d ", m.accelerations[i]);
	}
	printf("\n");
	printf("Battery measurements:\n");
	for (i = 0; i < m.batteriesCount; i = i + 1) {
		printf("%d ", m.batteries[i]);
	}
	printf("\n");
}

/*
 * Encode data structure to char array
 */
char * encode(struct Message m) {

	int size = getEncodeDataSize(m);
	char * message = malloc(size);
	int * data = encodeData(m);

	int i;
	for (i = 0; i < size; i += 1) {
		message[i] = data[i];
	}

	return message;
}

/*
 * Decodes array of ints into Message struct
 */
struct Message decode(char * message) {
	struct Message m;

	m.tempsCount = message[0];
	m.accelerationsCount = message[1];
	m.batteriesCount = message[2];

	int i;
	int j = 3;
	for (i = 0; i < m.tempsCount; i = i + 1) {
		m.temps[i] = message[j];
		j += 1;
	}
	for (i = 0; i < m.accelerationsCount; i = i + 1) {
		m.accelerations[i] = message[j];
		j += 1;
	}
	for (i = 0; i < m.batteriesCount; i = i + 1) {
		m.batteries[i] = message[j];
		j += 1;
	}

	return m;
}

/*
 * Resets Packets strructure 
 */
struct Packets resetPackets (struct Packets p) {
	p.count = 0;
	return p;
}

/*
 * Add Message to Packets buffer
 */
struct Packets addMessage (struct Packets p, struct Message message) {
	int count = p.count;
	// if the packets buffer is full ditch the last packet to make room for new one
	if (count == BUFFER_MAX_SIZE) {
		int i;
		for (i = 1; i < BUFFER_MAX_SIZE; i = i + 1) {
			p.payload[i - 1] = p.payload[i];
		}
		count = BUFFER_MAX_SIZE - 1;
	}
	p.payload[count] = message;
	p.count = count + 1;

	return p;
}


/*
 * Removes Message structure from Packets buffer
 */
struct Packets ackMessage (struct Packets p, int messageID) {
	int i;
	for (i = 0; i < p.count; i += 1) {
		if (p.payload[i].id == messageID)
			printf("Message with ID %d found on location %d\n", messageID, i);
	}

	int j;
	for (j = i; j < p.count - 1; j += 1) {
		p.payload[j] = p.payload[j + 1];
	}
	p.count -= 1;
	
	return p;

}

