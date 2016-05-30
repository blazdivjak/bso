#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_NEIGHBOURS 20
#define BUFFER_MAX_SIZE 5
// message send array is 128 bytes long
#define MESSAGE_BYTE_SIZE_MAX 35	// how many bytes a single message can take when completely full

// Commands - all have to be odd numbers for the last bit to be 1
#define CMD_SET_LOCAL_GW	1	// Set local group gateway. target_id = address of the new local gateway
#define CMD_QUERY_MOTE 		3   // Send status query to a specific mote
#define CMD_EMERGENCY_ONE	5	// Mote unreachable for more than 10sec. target_id is the address of unreachable mote
#define CMD_EMERGENCY_TWO	7	// Cow is running for more than 5sec, other motes should report fine grained RSSI data to that cow. target_id is address of running cow

/* 
 * struct and helper functions for storing measurements from sensors and sending 
 * data over network
 */
typedef struct Message {
	uint8_t id;			// message ID
	uint8_t mote_id;		// mote ID
	int8_t temp;			// current mote temp
	uint8_t battery;		//current mote battery level	
	uint8_t motionCount;
	uint64_t motions;		// bit encoded "array" of motions, 2 bit motion brackets (32 brackets)
	uint8_t neighbourCount;	// size of neighbour IDs array
	uint8_t neighbours[MAX_NEIGHBOURS];	// pointer to array of neighbour IDs
} Message;

void setMsgID (struct Message *m);
void addMotion (struct Message *m, uint8_t motion);
void getMotionArray (struct Message *m, uint8_t *buffer);
void addNeighbour (struct Message *m, uint8_t neighbour);
void resetMessage(struct Message *m);

/* send message is used like this:
* 	static uint8_t buffer[35];
*	uint8_t size = encodeData(&m, buffer);
*	packetbuf_copyfrom(buffer, size);   
*/

uint8_t encodeData(struct Message *m, uint8_t *buffer);
uint8_t getEncodeDataSize(struct Message *m);
void printMessage(struct Message *m);
void decode(uint8_t * buffer, uint8_t buffer_len, struct Message *m);

/* 
 * Struct and helper functions for packets monitoring
 */
typedef struct Packets {
	uint8_t count;
	struct Message payload[BUFFER_MAX_SIZE];
} Packets;

void resetPackets (struct Packets *p);
void addMessage (struct Packets *p, struct Message *message);
void ackMessage (struct Packets *p, uint8_t messageID);


typedef struct CmdMsg {
	uint8_t cmd;
	uint8_t id;
	uint8_t target_id;
} CmdMsg;

void encodeCmdMsg(struct CmdMsg *m, uint8_t *buffer);	// Always encodes it to 2 bytes (2 x uint8_t)
void decodeCmdMsg(uint8_t * buffer, struct CmdMsg *m);
uint8_t setCmdMsgId(struct CmdMsg *m, uint8_t id);
void printCmdMsg(struct CmdMsg *m);

#endif
