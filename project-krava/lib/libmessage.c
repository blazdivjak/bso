#ifndef __MESSAGE_H__
  #include "libmessage.h"
#endif


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
void setMsgID (struct Message *m) {
	m->id = (uint16_t) rand();
}


/*
 * Helper function for adding motion into struct
 */
void addMotion (struct Message *m, uint8_t motion) {
	if (m->motionCount < 32) 
		m->motionCount++;
	else 
		m->motionCount = 32;

	m->motions = (m->motions << 2) + (motion & 0x03); // shift motions by two and append the new motion to 2 LSBits
}

void getMotionArray (struct Message *m, uint8_t *buffer) {
	uint8_t i;
	uint64_t tmp = m->motions;
	for (i = 0; i < m->motionCount; i++) {
		buffer[i] = (uint8_t) (tmp & 0x03);
		tmp = tmp >> 2;
	}
}


void addNeighbour (struct Message *m, uint8_t neighbour) {	//can only add MAX_NEIGHBOURS neighbours
	if (m->neighbourCount < MAX_NEIGHBOURS) {
		m->neighbours[m->neighbourCount] = neighbour;
		m->neighbourCount++;
	}
}


/*
 * Reset struct Message and set values to empy
 */
void resetMessage(struct Message *m) {
	m->motionCount = 0;
	m->neighbourCount = 0;
	m->mote_id = 0;
	m->temp = 0;
	m->battery = 0;
	m->motions = 0;

	setMsgID(m);	// set a new ID
}
/*
 * Encode message struct into int array to be sent
 * buffer passed in has to be at least size MESSAGE_BYTE_SIZE_MAX
 * returns size of encoded data
 */
uint8_t encodeData(struct Message *m, uint8_t *buffer) {

	int8_t i = 0, j = 0;
	buffer[j++] = (uint8_t)((m->id) & 0xFF);
	buffer[j++] = (uint8_t)(((m->id) & 0xFF00)>>8);
	buffer[j++] = m->mote_id;
	buffer[j++] = m->temp;
	buffer[j++] = m->battery;

	buffer[j++] = m->motionCount;
	i = m->motionCount;
	uint64_t tmp = m->motions;
	do {
		buffer[j++] = (uint8_t)(tmp & 0xFF);
		tmp = tmp >> 8;
		i-=4;
	} while (i > 0);

	buffer[j++] = m->neighbourCount;
	for (i = 0; i < m->neighbourCount; i++) {
		buffer[j++] = m->neighbours[i];
	}

	return j;
}

/*
 * Returns encoded data size (size of int array)
 */
uint8_t getEncodeDataSize(struct Message *m) {
	return 7 + m->neighbourCount + (m->motionCount>>2) + ((m->motionCount & 0x03) == 0 ? 0 : 1);
}

/*
 * Prints data from Message struct
 */
void printMessage(struct Message *m) {
	//char buffer [m.tempsCount * 1 + m.accelerationsCount * 3 + m.batteriesCount * 1];
	//int bufferSize = 0;
	//bufferSize = sprintf(buffer, "%d plus %d is %d", a, b, a+b);
	printf("Message struct contains: %2d motion measurements and %2d neighbours \n", 
		m->motionCount, m->neighbourCount);
	printf("Msg ID: %d, Mote ID: %d, Temp: %d, Battery: %d\n", m->id, m->mote_id, m->temp, m->battery);

	printf("Motions:");
	uint8_t buffer[m->motionCount];
	getMotionArray(m, buffer);
	uint8_t i;
	for (i=0; i < m->motionCount; i++) {
		printf("%d ", buffer[i]);
	}

	printf("\nNeighbours: ");
	for (i=0; i < m->neighbourCount; i++) {
		printf("%d, ", m->neighbours[i]);
	}
	printf("\n");	
}

/*
 * Decodes array of ints into Message struct
 */
void decode(uint8_t * buffer, uint8_t buffer_len, struct Message *m)
{
	int8_t i = 0, j = 0;

	m->id = buffer[j++] + (buffer[j++]<<8);
	m->mote_id = buffer[j++];
	m->temp = buffer[j++];
	m->battery = buffer[j++];

	m->motionCount = buffer[j++];
	i = m->motionCount;
	m->motions = 0;
	uint8_t k = 0;
	do {
		m->motions += (((uint64_t)buffer[j++]) << (k*8));
		i-=4;
		k++;
	} while (i > 0);

	m->neighbourCount = buffer[j++];
	for (i = 0; i < m->neighbourCount; i++) {
		m->neighbours[i] = buffer[j++];
	}
}

// /*
//  * Resets Packets strructure 
//  */
// struct Packets resetPackets (struct Packets p) {
// 	p.count = 0;
// 	return p;
// }

// /*
//  * Add Message to Packets buffer
//  */
// struct Packets addMessage (struct Packets p, struct Message message) {
// 	int count = p.count;
// 	// if the packets buffer is full ditch the last packet to make room for new one
// 	if (count == BUFFER_MAX_SIZE) {
// 		int i;
// 		for (i = 1; i < BUFFER_MAX_SIZE; i = i + 1) {
// 			p.payload[i - 1] = p.payload[i];
// 		}
// 		count = BUFFER_MAX_SIZE - 1;
// 	}
// 	p.payload[count] = message;
// 	p.count = count + 1;

// 	return p;
// }


// /*
//  * Removes Message structure from Packets buffer
//  */
// struct Packets ackMessage (struct Packets p, int messageID) {
// 	int i;
// 	for (i = 0; i < p.count; i += 1) {
// 		if (p.payload[i].id == messageID)
// 			printf("Message with ID %d found on location %d\n", messageID, i);
// 	}

// 	int j;
// 	for (j = i; j < p.count - 1; j += 1) {
// 		p.payload[j] = p.payload[j + 1];
// 	}
// 	p.count -= 1;
	
// 	return p;

// }

