#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#define MESSAGE_MAX_SIZE 50
#define BUFFER_MAX_SIZE 5

/* 
 * struct and helper functions for storing measurements from sensors and sending 
 * data over network
 */
typedef struct Message {
	int id;
	int temps[MESSAGE_MAX_SIZE];
	int tempsCount;
	int accelerations[MESSAGE_MAX_SIZE];
	int accelerationsCount;
	int batteries[MESSAGE_MAX_SIZE];
	int batteriesCount;
} Message;

struct Message setID(struct Message m);
struct Message addTemperature (struct Message m, int temperature);
struct Message addAcceleration (struct Message m, int acceleration);
struct Message addBattery (struct Message m, int battery);
struct Message resetMessage(struct Message m);
int * encodeData(struct Message m);
int getEncodeDataSize(struct Message m);
void printMessage(struct Message m);
char * encode(struct Message m);
struct Message decode(char * message);

/* 
 * Struct and helper functions for packets monitoring
 */
typedef struct Packets {
	int count;
	struct Message payload[BUFFER_MAX_SIZE];
} Packets;

struct Packets resetPackets (struct Packets p);
struct Packets addMessage (struct Packets p, struct Message message);
struct Packets ackMessage (struct Packets p, int messageID);

#endif
