#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#define MESSAGE_MAX_SIZE 50

//struct Message;
typedef struct Message {
	int temps[MESSAGE_MAX_SIZE];
	int tempsCount;
	int accelerations[MESSAGE_MAX_SIZE];
	int accelerationsCount;
	int batteries[MESSAGE_MAX_SIZE];
	int batteriesCount;
} Message;

struct Message addTemperature (struct Message m, int temperature);
struct Message addAcceleration (struct Message m, int acceleration);
struct Message addBattery (struct Message m, int battery);
struct Message resetMessage(struct Message m);
int * encodeData(struct Message m);
int getEncodeDataSize(struct Message m);
void printMessage(struct Message m);
char * encode(struct Message m);
struct Message decode(char * message);

#endif
