#ifndef __KRAVA_H__
#define __KRAVA_H__

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/i2cmaster.h"  // Include IC driver

#include "dev/battery-sensor.h"
#include "net/rime/rime.h"
#include "net/rime/mesh.h"
#include "sys/node-id.h"
#include "../lib/libsensors.h"
#include "../lib/libmessage.h"
#include "random.h"
#include "cc2420.h"
//#include "../lib/libmath.h"

#define MOVEMENT_READ_INTERVAL (CLOCK_SECOND)
#define RSSI_READ_INTERVAL (CLOCK_SECOND)*5
#define MESH_REFRESH_INTERVAL (CLOCK_SECOND)*360
#define TEMP_READ_INTERVAL (CLOCK_SECOND)*30
#define BATTERY_READ_INTERVAL (CLOCK_SECOND)*30
#define SEND_INTERVAL (CLOCK_SECOND)*30
#define ACK_COUNT_INTERVAL (CLOCK_SECOND)*120

static unsigned long mesh_refresh_interval = MESH_REFRESH_INTERVAL;
static unsigned long send_interval = SEND_INTERVAL/2;
static unsigned long movement_read_interval = MOVEMENT_READ_INTERVAL;
static unsigned long rssi_read_interval = RSSI_READ_INTERVAL;
static unsigned long temp_read_intreval = TEMP_READ_INTERVAL;
static unsigned long battery_read_interval = BATTERY_READ_INTERVAL;
static unsigned long ack_count_interval = ACK_COUNT_INTERVAL;

//Networking
#define DEFAULT_GATEWAY_ADDRESS 0 //falback to default gateway if we cant conntact gateway
#define CURRENT_GATEWAY_ADDRESS 0
#define MY_ADDRESS_1 0//1
#define MY_ADDRESS_2 0//1

static uint8_t myAddress_1 = MY_ADDRESS_1;
static uint8_t myAddress_2 = MY_ADDRESS_2;
static uint8_t defaultGateway = DEFAULT_GATEWAY_ADDRESS;
static uint8_t currentGateway = CURRENT_GATEWAY_ADDRESS;
static uint8_t sendFailedCounter = 0;

//Neighbors and definitions
#define NEIGHBOR_TABLE_REINITIALIZE_INTERVAL (CLOCK_SECOND)*360
#define NEIGHBOR_SENSE_INTERVAL (CLOCK_SECOND)*5
#define NEIGHBOR_SENSE_TIME (CLOCK_SECOND)*1
#define NEIGHBOR_ADVERTISEMENT_INTERVAL (CLOCK_SECOND)*5
#define RSSI_TRESHOLD -75

static unsigned long neighbor_table_reinitialize_interval = NEIGHBOR_TABLE_REINITIALIZE_INTERVAL;
static unsigned long neighbor_sense_interval = NEIGHBOR_SENSE_INTERVAL;
static unsigned long  neighbor_sense_time = NEIGHBOR_SENSE_TIME;
static unsigned long  neighbor_advertisment_interval = NEIGHBOR_ADVERTISEMENT_INTERVAL;

static uint8_t kravaNeighbors[20];
static uint8_t numberOfNeighbors = 0;

//message buffer
static uint8_t send_buffer[MESSAGE_BYTE_SIZE_MAX];
static uint8_t command_buffer[CMD_BUFFER_MAX_SIZE];
static uint8_t emergencyBuffer[EMERGENCY_DATA_MAX+3];
static int rssiTreshold = RSSI_TRESHOLD;
Message m; //message we save to
Message mNew; //new message received for decoding
Packets myPackets; //list of packets sent and waiting to be acked
Packets otherKravaPackets; //list of packets sent from other kravas if I am gateway
CmdMsg command;
EmergencyMsg eTwoRSSI, eTwoAcc;

//Measurement
#define IIR_STRENGTH 4
#define MOVEMENT_COUNTER_VAL 2
#define RUNNING_MAX 5
#define WALKING_TRESHOLD 100000
#define RUNNING_TRESHOLD 200000
static int64_t average_movement = 70000;
static uint8_t movement_counter = 0;
static uint8_t running_counter = 0;

//Power
static uint8_t txpower;

struct {	
	uint8_t iAmGateway : 1;
	uint8_t ackCounter : 1;
	uint8_t emergencyOne : 2;
	uint8_t emergencyTwo : 2;
	uint8_t emergencyTarget : 1;
} status;

/*
* Mesh functions
*/
static void sent(struct mesh_conn *c);
static void timedout(struct mesh_conn *c);
static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops);
/*
* Initialize mesh
*/
static struct mesh_conn mesh;
const static struct mesh_callbacks callbacks = {recv, sent, timedout};


// Handle gateway commands
void handleCommand(CmdMsg *command);
// Emergency mode handling
void handleEmergencyOne();
void handleEmergencyTwo();
void cancelEmergencies();
void toggleEmergencyOne();
static void triggerEmergencyTwo();
static void cancelEmergencyTwo();
void setPower(uint8_t powerLevel);


/*
* Communication functions
*/
static void setAddress(uint8_t myAddress_1, uint8_t myAddress_2);
static void setCurrentGateway(uint8_t currentGatewayAddress);
void sendCommand();
void sendMessage();
void sendEmergencyTwoRSSI();
void sendEmergencyTwoAcc();

/*
* Initialize broadcast connection
*/
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from);
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;

/*
* Sensor functions															 
*/
void readBattery();
void readTemperature();
void readMovement();
int readRSSI();

#endif