#ifndef __MESSAGE_H__
#define __MESSAGE_H__

struct Message;

void addTemperature (struct Message m, int temperature);
void addAcceleration (struct Message m, int acceleration);
void addBattery (struct Message m, int battery);

#endif
