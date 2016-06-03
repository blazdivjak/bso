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
uint8_t setMsgId (struct Message *m, uint8_t id) {
	if (id > 0x3F) {
		id = id % 0x3F;
	}
	m->id = (id<<2) | MSG_MESSAGE;
	return id;
}

uint8_t getMsgId(struct Message *m) {
	return (m->id>>2)&0x3F;
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
	uint8_t i = 0;
	for (;i<m->neighbourCount; i++) {
		if (neighbour == m->neighbours[i]) {
			return;
		}
	}

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

	setMsgId(m, getMsgId(m)+1);	// set a new ID
}


/*
 * Encode message struct into int array to be sent
 * buffer passed in has to be at least size MESSAGE_BYTE_SIZE_MAX
 * returns size of encoded data
 */
uint8_t encodeData(struct Message *m, uint8_t *buffer) {

	int8_t i = 0, j = 0;
	buffer[j++] = (uint8_t)((m->id) & 0xFF);
	//buffer[j++] = (uint8_t)(((m->id) & 0xFF00)>>8);
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
	return 6 + m->neighbourCount + (m->motionCount>>2) + ((m->motionCount & 0x03) == 0 ? 0 : 1);
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
	printf("Msg ID: %d, Mote ID: %d, Temp: %d, Battery: %d\n", getMsgId(m), m->mote_id, m->temp, m->battery);

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

	// m->id = buffer[j++] + (buffer[j++]<<8);
	m->id = buffer[j++];
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

/*
 * Resets Packets strructure 
 */
void resetPackets (struct Packets *p) {
	p->count = 0;
}

/*
 * Add Message to Packets buffer
 */
void addMessage (struct Packets *p, struct Message *message) {
	// if the packets buffer is full ditch the last packet to make room for new one
	if (p->count == BUFFER_MAX_SIZE) {
		uint8_t i;
		for (i = 1; i < BUFFER_MAX_SIZE; i++) {
			p->payload[i - 1] = p->payload[i];
		}
		p->count--;
	}
	p->payload[p->count] = *message;
	p->count++;
}


/*
 * Removes Message structure from Packets buffer
 */
void ackMessage (struct Packets *p, uint8_t messageID) {
	uint8_t i;
	for (i = 0; i < p->count; i += 1) {
		if (p->payload[i].id == messageID) {
			// printf("Message with ID %d found on location %d\n", messageID, i);
			break;
		}
	}
	if (i == p->count)
		return;

	for (; i < p->count - 1; i++) {
		p->payload[i] = p->payload[i+1];
	}
	p->count--;
}

// Always encodes it to 3 bytes (3 x uint8_t)
void encodeCmdMsg(struct CmdMsg *m, uint8_t *buffer) {
	buffer[0] = m->id;
	buffer[1] = m->cmd;
	buffer[2] = m->target_id;
}

void decodeCmdMsg(uint8_t * buffer, struct CmdMsg *m) {
	m->id = buffer[0];
	m->cmd = buffer[1];
	m->target_id = buffer[2];
}

uint8_t setCmdMsgId(struct CmdMsg *m, uint8_t id) {
	if (id > 0x3F) {
		id = id % 0x3F;
	}
	m->id = (id<<2) | MSG_CMD ;
	return id;
}

uint8_t getCmdMsgId(struct CmdMsg *m) {
	return (m->id>>2);
}

void resetCmdMsg(struct CmdMsg *m) {
	setCmdMsgId(m, getCmdMsgId(m)+1);
}

void printCmdMsg(struct CmdMsg *m) {
	if (m->cmd == CMD_SET_LOCAL_GW) {
		printf("Set Local gateway command: ");
	} else if (m->cmd == CMD_QUERY_MOTE) {
		printf("Query mote command: ");
	} else if (m->cmd == CMD_EMERGENCY_ONE) {
		printf("Emergency mode 1: ");
	} else if (m->cmd == CMD_EMERGENCY_TWO) {
		printf("Emergency mode 2: ");
	}
	printf("Message ID = %d ", getCmdMsgId(m));
	printf("Target ID = %d\n", m->target_id);
}

uint8_t encodeEmergencyMsg(struct EmergencyMsg *m, uint8_t *buffer) {

	int8_t i = 0, j = 0;
	buffer[j++] = m->id;
	buffer[j++] = m->mote_id;

	buffer[j++] = m->dataCount;
	for (i = 0; i < m->dataCount; i++) {
		buffer[j++] = m->data[i];
	}

	return j;
}

void decodeEmergencyMsg(uint8_t * buffer, struct EmergencyMsg *m)
{
	int8_t i = 0, j = 0;

	m->id = buffer[j++];
	m->mote_id = buffer[j++];

	m->dataCount = buffer[j++];
	for (i = 0; i < m->dataCount; i++) {
		m->data[i] = buffer[j++];
	}
}

uint8_t setEmergencyMsgId(struct EmergencyMsg *m, uint8_t id) {
	if (id > 0x3F) {
		id = id % 0x3F;
	}
	m->id = (id<<2) | (m->id & 0x03);
	return id;
}

uint8_t getEmergencyMsgId(struct EmergencyMsg *m){
	return (m->id>>2);
}

void setEmergencyMsgType(struct EmergencyMsg *m, uint8_t type){
	m->id = (m->id&0xFC) | (type&0x03);
}

uint8_t getEmergencyMsgType(struct EmergencyMsg *m) {
	return (m->id)&0x03;
}

void printEmergencyMsg(struct EmergencyMsg *m) {
	uint8_t i;
	if (getEmergencyMsgType(m) == MSG_E_TWO_RSSI) {
		printf("Emergency two msg: id=%d; RSSI Count=%d, RSSIs:\n", getEmergencyMsgId(m), m->dataCount);
		for (i=0; i < m->dataCount; i++) {
			printf("-%d, ", m->data[i]);
		}
		printf("\n");
	} else if (getEmergencyMsgType(m) == MSG_E_TWO_ACC) {
		printf("Emergency two msg: id=%d; accelerations Count=%d, accelerations:\n", getEmergencyMsgId(m), m->dataCount);
		for (i=0; i < m->dataCount; i++) {
			printf("%d, ", m->data[i]);
		}
		printf("\n");
	} else {
		printf("Unsupported emergency type\n");
		return;
	}
}

uint8_t addEmergencyData(struct EmergencyMsg *m, uint8_t dataPoint) {
	// uint8_t i = 0;

	if (m->dataCount < EMERGENCY_DATA_MAX) {
		m->data[m->dataCount] = dataPoint;
		m->dataCount++;
	}

	return m->dataCount;
}

void resetEmergencyMsg(struct EmergencyMsg *m) {
	setEmergencyMsgId(m, getEmergencyMsgId(m)+1);
	m->dataCount = 0;
}
