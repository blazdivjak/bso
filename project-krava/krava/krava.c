/*
*__authors: blaz, gregor, gasper__
*__date: 2016-04-15__
*__project krava__
*/
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "contiki.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/i2cmaster.h"  // Include IC driver
#include "dev/tmp102.h"     // Include sensor driver
#include "dev/battery-sensor.h"
#include "dev/adxl345.h"
#include "net/rime/rime.h"
#include "net/rime/mesh.h"
#include "sys/node-id.h"
#include "../lib/libsensors.h"
#include "../lib/libmessage.h"
#include "random.h"
//#include "../lib/libmath.h"

#define MOVEMENT_READ_INTERVAL (CLOCK_SECOND)/2
#define RSSI_READ_INTERVAL (CLOCK_SECOND)*5
#define MESH_REFRESH_INTERVAL (CLOCK_SECOND)*360
#define TEMP_READ_INTERVAL (CLOCK_SECOND)*30
#define BATTERY_READ_INTERVAL (CLOCK_SECOND)*30
#define SEND_INTERVAL (CLOCK_SECOND)*30

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

//Neighbors
#define NEIGHBOR_TABLE_REINITIALIZE_INTERVAL (CLOCK_SECOND)*360
#define NEIGHBOR_SENSE_INTERVAL (CLOCK_SECOND)*5
#define NEIGHBOR_SENSE_TIME (CLOCK_SECOND)*1
#define NEIGHBOR_ADVERTISEMENT_INTERVAL (CLOCK_SECOND)*5
#define RSSI_TRESHOLD -75
static uint8_t kravaNeighbors[20];
static uint8_t numberOfNeighbors = 0;

//message buffer
static uint8_t send_buffer[MESSAGE_BYTE_SIZE_MAX];
static uint8_t command_buffer[CMD_BUFFER_MAX_SIZE];
static int rssiTreshold = RSSI_TRESHOLD;
Message m; //message we save to
Message mNew; //new message received for decoding
Packets myPackets; //list of packets sent and waiting to be acked
Packets otherKravaPackets; //list of packets sent from other kravas if I am gateway
CmdMsg command;

//Measurement
#define IIR_STRENGTH 4
#define MOVEMENT_COUNTER_VAL 2
#define RUNNING_MAX 5
#define WALKING_TRESHOLD 100000
#define RUNNING_TRESHOLD 200000
static int64_t average_movement = 70000;
static uint8_t movement_counter = 0;
static uint8_t running_counter = 0;

struct {
	unsigned char iAmGateway : 1;
	unsigned char emergencyOne : 1;
	unsigned char emergencyTwo : 1;
} status;

/*
* Mesh functions
*/

static void sent(struct mesh_conn *c)
{
  printf("packet sent\n");
}
static void timedout(struct mesh_conn *c)
{
  //TODO: Move this function to ACK receive function
  sendFailedCounter += 1;
  printf("packet timedout. Failed to send packet counter: %d\n", sendFailedCounter);
}

static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops){
   
  printf("Data received from %d.%d: %d bytes\n",from->u8[0], from->u8[1], packetbuf_datalen());
  
  //ACK
  if(packetbuf_datalen()==1){
  	printf("Message ID: %d ACK received.\n", ((uint8_t *)packetbuf_dataptr())[0]);
  	ackMessage(&myPackets, ((uint8_t *)packetbuf_dataptr())[0]);
  }
  //Krava message
  else if((((uint8_t *)packetbuf_dataptr())[0] & 0x01) == 0){
  	decode(((uint8_t *)packetbuf_dataptr()), packetbuf_datalen(), &mNew);
  	printMessage(&mNew);

  	//TODO: If I am gateway add this packets to otherKravaPackets
  }
  //Gateway command
  else{  	
  	CmdMsg command;
    decodeCmdMsg(packetbuf_dataptr(), &command);
    handleCommand(&command);
  }    
}

/*
Handle gateway commands
*/

void handleCommand(CmdMsg *command) {
  
  if (command->cmd == CMD_SET_LOCAL_GW) {
    printf("Command: Set local gateway: %d", command->target_id);
  }else if (command->cmd == CMD_QUERY_MOTE) {
    printf("Command: Query from gateway: %d", command->target_id);
  }else if (command->cmd == CMD_EMERGENCY_ONE) {
    printf("Emergency one, cow unreachable id: %d", command->target_id);
  } else if (command->cmd == CMD_EMERGENCY_TWO) {
    printf("Emergency two, cow running id: %d", command->target_id);
  } else if (command->cmd == CMD_CANCEL_EMERGENCY_ONE) {
    printf("Emergency one cancel, cow id: %d", command->target_id);
  } else if (command->cmd == CMD_CANCEL_EMERGENCY_TWO) {
    printf("Emergency two cancel, cow id: %d", command->target_id);
  }
}

/*
Emergency mode handling
*/
static void triggerEmergencyTwo(){
	if (status.emergencyTwo == 1) {
		
		return;
	}
	status.emergencyTwo = 1;
	printf("Emergency Two triggered\n");

	setCmdMsgId(&command, 32);
	command.cmd = CMD_EMERGENCY_TWO;
	command.target_id = myAddress_1;
	sendCommand();	

}

static void cancelEmergencyTwo(){
	running_counter = 0;
	if (status.emergencyTwo == 0) {
		return;
	}
	printf("Emergency Two canceled\n");
	status.emergencyTwo = 0;
	setCmdMsgId(&command, 32);
	command.cmd = CMD_CANCEL_EMERGENCY_TWO;
	command.target_id = myAddress_1;
	sendCommand();		
}

/*
* Initialize mesh
*/
static struct mesh_conn mesh;
const static struct mesh_callbacks callbacks = {recv, sent, timedout};

/*
* Communication functions
*/
static void setAddress(uint8_t myAddress_1, uint8_t myAddress_2){  
  m.mote_id = myAddress_1;
  linkaddr_t addr;
  addr.u8[0] = myAddress_1;
  addr.u8[1] = myAddress_2;
  printf("My Address: %d.%d\n", addr.u8[0],addr.u8[1]);
  uint16_t shortaddr = (addr.u8[0] << 8) + addr.u8[1];
  cc2420_set_pan_addr(IEEE802154_PANID, shortaddr, NULL);   
  linkaddr_set_node_addr (&addr);	
}

static void setCurrentGateway(uint8_t currentGatewayAddress){

	currentGateway = currentGatewayAddress;
}


void sendCommand(){
	
  printf("Sending command to my current gateway ID: %d.0\n", currentGateway);
    
  linkaddr_t addr_send;     
  encodeCmdMsg(&command, &command_buffer);  
  packetbuf_copyfrom(command_buffer, CMD_BUFFER_MAX_SIZE);       
  addr_send.u8[0] = currentGateway;
  addr_send.u8[1] = 0;  
  mesh_send(&mesh, &addr_send);
}

static void sendMessage(){
	
  printf("Sending message to my current gateway ID: %d.0\n", currentGateway);

  setMsgID(&m);
  
  //Copy message  
  addMessage(&myPackets, &m);

  //TODO: Poslji komplet pakete

  linkaddr_t addr_send; 
  //char msg[2] = {1, 2};
  //packetbuf_copyfrom(msg, 2);
  uint8_t size = encodeData(&m, &send_buffer);
  packetbuf_copyfrom(send_buffer, size);       
  addr_send.u8[0] = currentGateway;
  addr_send.u8[1] = 0;
  //printf("Mesh status: %d\n", mesh_ready(&mesh));
  mesh_send(&mesh, &addr_send);
}

/*
* Initialize broadcast connection
*/

static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{	

  int rssi;	
  rssi = readRSSI();
  //printf("Neighbor advertisment received from %d.%d: '%s' RSSI: %d\n", from->u8[0], from->u8[1], (char *)packetbuf_dataptr(), rssi);
    
  //Check message rssi for adding only close neighbors  
  if (rssi>=rssiTreshold){
  	addNeighbour(&m, from->u8[0]);
	printf("Neighbor detected adding %d to table. RSSI: %d, Current number of neighbors: %d.\n",from->u8[0],rssi, m.neighbourCount);
  }    
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;

/*
/* Sensor functions															 
*/
void readBattery(){
  
    //uint16_t bateria = battery_sensor.value(0);
    //float mv = (bateria * 2.500 * 2) / 4096;
    //printf("Battery: %i (%ld.%03d mV)\n", bateria, (long)mv,(unsigned)((mv - myFloor(mv)) * 1000));
    m.battery = decodeBattery(battery_sensor.value(0));

}  
void readTemperature(){
	
	static int8_t decoded_temperature;
	decoded_temperature =  decodeTemperature(tmp102_read_temp_raw());	
	//printTemperature(decoded_temperature);
	m.temp = decoded_temperature;
}


void readMovement(){
	
	int64_t x, y, z;	

	x = adxl345.value(X_AXIS);
    y = adxl345.value(Y_AXIS);
    z = adxl345.value(Z_AXIS);       
    //printf("Movement: x: %d y: %d z: %d\n",x, y, z);  
    int64_t acc =  x*x + y*y + z*z;
    average_movement = average_movement + (acc - average_movement)/IIR_STRENGTH;
    if (movement_counter%MOVEMENT_COUNTER_VAL == 0) {
	    if (average_movement < WALKING_TRESHOLD) {
	    	addMotion(&m, STANDING);
	    	printf("Standing \t");
	    	cancelEmergencyTwo();
	    } else if (average_movement < RUNNING_TRESHOLD) {
	    	addMotion(&m, WALKING);
	    	printf("Walking \t");
	    	cancelEmergencyTwo();
	    } else {
	    	addMotion(&m, RUNNING);
	    	printf("Running \t");
	    	if (running_counter != 0xFF) {
	    		running_counter++;
	    	}
	    }

	    if (running_counter >= RUNNING_MAX) {
	    	triggerEmergencyTwo();
	    }

	    printf("Acce: %" PRId64 "\tAvg: %" PRId64 "\n", acc, average_movement);
	}
	movement_counter++;

    
    //TODO: Compare with previous motion and find 0,1,2,3 motion statuses.

}
int readRSSI(){
		
  return packetbuf_attr(PACKETBUF_ATTR_RSSI) - 45;
}

/*
*Process definitions
*/
PROCESS(krava, "Krava");
PROCESS(communication, "Communication");
PROCESS(neighbors, "Sense neigbors");
AUTOSTART_PROCESSES(&krava, &communication, &neighbors);
/*
* Krava process
*/
PROCESS_THREAD(krava, ev, data)
{	
	//Our process
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();
	printf("Sensor sensing process\n");
	status.iAmGateway = 0;
	status.emergencyOne = 0;
	status.emergencyTwo = 0;

	//Initialize timers for intervals
	static struct etimer movementReadInterval;
	static struct etimer temperatureReadInterval;	
	static struct etimer rssiReadInterval;

	etimer_set(&movementReadInterval, MOVEMENT_READ_INTERVAL);
	etimer_set(&rssiReadInterval, RSSI_READ_INTERVAL);
	etimer_set(&temperatureReadInterval, TEMP_READ_INTERVAL);	
	
	//Initialize sensors
	tmp102_init();
	
	SENSORS_ACTIVATE(button_sensor);	
	SENSORS_ACTIVATE(battery_sensor);
	SENSORS_ACTIVATE(adxl345);

	//Process main loop
	while(1) {

		PROCESS_WAIT_EVENT();
		
		if(etimer_expired(&movementReadInterval)){
			readMovement();
			etimer_reset(&movementReadInterval);
		}		
		if(etimer_expired(&temperatureReadInterval)){
			readTemperature();
			readBattery();			
			etimer_reset(&temperatureReadInterval);
		}		
	}
	exit:		
		SENSORS_DEACTIVATE(button_sensor);
		SENSORS_DEACTIVATE(battery_sensor);	
		SENSORS_DEACTIVATE(adxl345);	
		leds_off(LEDS_ALL);
		PROCESS_END();
}

/*
* communication process
*/
PROCESS_THREAD(communication, ev, data)
{	
	//Our process		
	PROCESS_BEGIN();
	resetMessage(&m);
	printf("Communication process\n");	
	setAddress(node_id, myAddress_2);
	setCurrentGateway(defaultGateway);
		
	static struct etimer sendInterval;
	static struct etimer meshRefreshInterval;	
	
	etimer_set(&sendInterval, SEND_INTERVAL);
	etimer_set(&meshRefreshInterval, MESH_REFRESH_INTERVAL);	
	mesh_open(&mesh, 14, &callbacks);

	//Process main loop
	while(1) {
	 
		PROCESS_WAIT_EVENT();
		//set address to address in settings
		if(ev == sensors_event && data == &button_sensor){
      		setAddress(myAddress_1, myAddress_2);
    	}		
    	//sent message
		if(etimer_expired(&sendInterval)){
			sendMessage();
			etimer_reset(&sendInterval);
		}
		//reinitialize mesh if sending failed more than 5 times
		//TODO: SendFailedCounter=queue length
		//if(sendFailedCounter%10==0){
		if(etimer_expired(&meshRefreshInterval)){
			printf("Closing Mesh\n");			
			mesh_close(&mesh);
			m.neighbourCount = 0;
			mesh_open(&mesh, 14, &callbacks);
			printf("Initializing Mesh\n");			
			//sendFailedCounter+=10;
			etimer_reset(&meshRefreshInterval);
		}
	}	
	PROCESS_END();
}

/*
Sense neighbors around and sent packet to broadcast so others can sense you krava :)
*/
PROCESS_THREAD(neighbors, ev, data)
{	
	//Our process	
	PROCESS_BEGIN();
	printf("Neighbor discovery process\n");

	static struct etimer neighborAdvertismentInterval;
	static struct etimer neighborSenseInterval;
	static struct etimer neighborSenseTime;
	static struct etimer neighborTableRinitializeInterval;
	etimer_set(&neighborAdvertismentInterval, NEIGHBOR_ADVERTISEMENT_INTERVAL);
	etimer_set(&neighborSenseInterval, NEIGHBOR_SENSE_INTERVAL);
	etimer_set(&neighborSenseTime, NEIGHBOR_SENSE_TIME);
	etimer_set(&neighborTableRinitializeInterval, NEIGHBOR_TABLE_REINITIALIZE_INTERVAL);	
	uint8_t sensing = 0;	
	//Process main loop
	while(1) {
		PROCESS_WAIT_EVENT();	
		
		//sense neighbors every cca 5s for 1s
		if(etimer_expired(&neighborSenseInterval)){
			if(sensing==0){
				broadcast_open(&broadcast, 129, &broadcast_call);
				//printf("Sensing for neighbors\n");								
				etimer_set(&neighborSenseTime, NEIGHBOR_SENSE_TIME);
				sensing = 1;
			}						
		}
		//Stop sensing after 1s
		if(etimer_expired(&neighborSenseTime)){
			if(sensing == 1){
				broadcast_close(&broadcast);
				//printf("Sensing stoped\n");					
				etimer_set(&neighborSenseInterval, NEIGHBOR_SENSE_INTERVAL + random_rand() % (NEIGHBOR_SENSE_INTERVAL));
				sensing = 0;
			}						
		}								
			
		//send advertisment to your neighbors every 5s
		if(etimer_expired(&neighborAdvertismentInterval)){
			broadcast_open(&broadcast, 129, &broadcast_call);
 			packetbuf_copyfrom("Hello", 5);    
    		broadcast_send(&broadcast);
    		//printf("Neighbor advertisment sent\n");
			etimer_set(&neighborAdvertismentInterval, NEIGHBOR_SENSE_INTERVAL + random_rand() % (NEIGHBOR_SENSE_INTERVAL));
			broadcast_close(&broadcast);
		}				
	}
	PROCESS_END();
}
