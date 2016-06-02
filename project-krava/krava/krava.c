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

//Power
static uint8_t txpower;

struct {	
	uint8_t iAmGateway;
	uint8_t ackCounter;
	uint8_t emergencyOne;
	uint8_t emergencyTwo;
	uint8_t emergencyTarget;
	uint8_t systemEmergencyOne;
	uint8_t systemEmergencyTwo;
} status;

/*
* Mesh functions
*/

static void sent(struct mesh_conn *c)
{
  printf("NETWORK: packet sent\n");
}
static void timedout(struct mesh_conn *c)
{
  //TODO: Move this function to ACK receive function
  sendFailedCounter += 1;
  printf("NETWORK: packet timedout. Failed to send packet counter: %d\n", sendFailedCounter);
}

static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops){
   
  printf("MESSAGES: Data received from %d.%d: %d bytes\n",from->u8[0], from->u8[1], packetbuf_datalen());
  
  //ACK
  if(packetbuf_datalen()==1){
  	printf("MESSAGES: Message ID: %d ACK received.\n", ((uint8_t *)packetbuf_dataptr())[0]);
  	ackMessage(&myPackets, ((uint8_t *)packetbuf_dataptr())[0]);
  	status.ackCounter+=1;
  }
  //Krava message
  else if((((uint8_t *)packetbuf_dataptr())[0] & 0x01) == 0){
  	decode(((uint8_t *)packetbuf_dataptr()), packetbuf_datalen(), &mNew);
  	printMessage(&mNew);

  	//TODO: If I am gateway add this packets to otherKravaPackets

  	//TODO: Send ACK for packet

  	//TODO: Aggregate packets and send forward

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
  
  printf("COMMAND: Processing command\n"); 	

  if(command->cmd == CMD_SET_LOCAL_GW) {
    printf("COMMAND: Set local gateway: %d\n", command->target_id);
    currentGateway = command->target_id;
    setPower(15);

    //TODO: Set TX power

  } else if (command->cmd == CMD_QUERY_MOTE) {
    printf("COMMAND: Query from gateway: %d\n", command->target_id);
    sendMessage();
  } else if (command->cmd == CMD_EMERGENCY_ONE) {
    printf("COMMAND: Emergency one, cow unreachable id: %d\n", command->target_id);
    handleEmergencyOne();
  } else if (command->cmd == CMD_EMERGENCY_TWO) {
    printf("COMMAND: Emergency two, cow running id: %d\n", command->target_id);
    handleEmergencyTwo();
  } else if (command->cmd == CMD_CANCEL_EMERGENCY_ONE) {
    printf("COMMAND: Emergency one cancel, cow id: %d\n", command->target_id);
    cancelEmergencies();
  } else if (command->cmd == CMD_CANCEL_EMERGENCY_TWO) {
    printf("COMMAND: Emergency two cancel, cow id: %d\n", command->target_id);
    cancelEmergencies();
  }
}

/*
Emergency mode handling
*/

void handleEmergencyOne(){

	//Reconfigure timers
	printf("EMERGENCY: Searching for lost krava.\n");
	mesh_refresh_interval = (CLOCK_SECOND)*30;

	//full power
	setPower(31);	

}

void handleEmergencyTwo(){

	//If I am the running krava dont bother to monitor :)
	if(status.emergencyTwo==1){
		return;
	}
	//Monitor rssi for this krava
	else{
		printf("EMERGENCY: Starting running krava monitoring.\n");

		//Configure broadcast listening timer and sense more offten
		neighbor_sense_time = (CLOCK_SECOND);		
		
		//TODO: save its RSSI to table and sent to Gateway

	}	
}

void cancelEmergencies(){

	//TODO: Reconfigure timers and back to normal operations with all timers
	printf("Resuming normal operations.\n");

	neighbor_sense_time =  NEIGHBOR_SENSE_TIME;
	mesh_refresh_interval = MESH_REFRESH_INTERVAL;
	
	if(status.emergencyOne==2){
		
		//TODO: Back to previous power level
		setPower(txpower);
	}
	else if(status.emergencyTwo==2){
		//TODO: Send data
	}


}

void toggleEmergencyOne(){
	
	if (status.emergencyOne == 1) {		
		return;
	}

	if(status.ackCounter==0){
		printf("EMERGENCY: Emergency One triggered.\n");
		status.emergencyOne = 1;
		currentGateway = DEFAULT_GATEWAY_ADDRESS;
		mesh_refresh_interval = (CLOCK_SECOND)*30;
		//Full power & mesh reinitialize
		setPower(31);		

	}else{		
		status.ackCounter=0;	
		/*if(status.emergencyOne==1){
			printf("EMERGENCY: Emergency One canceled.\n");
			status.emergencyOne=0;
			mesh_refresh_interval = MESH_REFRESH_INTERVAL;
		}*/
		status.emergencyOne=0;
		mesh_refresh_interval = MESH_REFRESH_INTERVAL;					
	}	
}

static void triggerEmergencyTwo(){
	
	if (status.emergencyTwo == 1) {		
		return;
	}
	
	status.emergencyTwo = 1;
	printf("EMERGENCY: Emergency Two triggered\n");

	setCmdMsgId(&command, 32);
	command.cmd = CMD_EMERGENCY_TWO;
	command.target_id = m.mote_id;
	sendCommand();	

	//Advertise offten and mesure movement more offten	
	neighbor_advertisment_interval = (CLOCK_SECOND)/2;
	movement_read_interval = (CLOCK_SECOND)/4;

}

static void cancelEmergencyTwo(){
	
	running_counter = 0;
	
	if (status.emergencyTwo == 0) {
		return;
	}
	
	printf("EMERGENCY: Emergency Two canceled\n");
	status.emergencyTwo = 0;
	setCmdMsgId(&command, 32);
	command.cmd = CMD_CANCEL_EMERGENCY_TWO;
	command.target_id = m.mote_id;
	sendCommand();
	
	neighbor_advertisment_interval = NEIGHBOR_ADVERTISEMENT_INTERVAL;
	movement_read_interval = MOVEMENT_READ_INTERVAL;
}

/*
Power handling
*/

void setPower(uint8_t powerLevel){
		
	txpower = cc2420_get_txpower();
	printf("POWER: previous: %d Now: %d\n", txpower, powerLevel);
	cc2420_set_txpower(powerLevel);	

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
  printf("NETWORK: My Address: %d.%d\n", addr.u8[0],addr.u8[1]);
  uint16_t shortaddr = (addr.u8[0] << 8) + addr.u8[1];
  cc2420_set_pan_addr(IEEE802154_PANID, shortaddr, NULL);   
  linkaddr_set_node_addr (&addr);	
}

static void setCurrentGateway(uint8_t currentGatewayAddress){

	printf("NETWORK: Setting current gateway to: %d\n", currentGatewayAddress);
	currentGateway = currentGatewayAddress;
}

void sendCommand(){
	
  printf("COMMAND: Sending command to my current gateway ID: %d.0\n", currentGateway);
    
  linkaddr_t addr_send;     
  encodeCmdMsg(&command, &command_buffer);  
  packetbuf_copyfrom(command_buffer, CMD_BUFFER_MAX_SIZE);       
  addr_send.u8[0] = currentGateway;
  addr_send.u8[1] = 0;  
  mesh_send(&mesh, &addr_send);
}

void sendMessage(){
	
  printf("MESSAGES: Sending message to my current gateway ID: %d.0\n", currentGateway);

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
      
  //If emergency
  if((status.emergencyTwo) == 2 && (status.emergencyTarget == from->u8[0])){
  	
  	//TODO: Save to RSSI mesurements for lost krava

  }  

  //Check message rssi for adding only close neighbors  
  if (rssi>=rssiTreshold){
  	addNeighbour(&m, from->u8[0]);
	printf("NETWORK: Neighbor detected adding %d to table. RSSI: %d, Current number of neighbors: %d.\n",from->u8[0],rssi, m.neighbourCount);
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
	    	//printf("Standing \t");
	    	cancelEmergencyTwo();
	    } else if (average_movement < RUNNING_TRESHOLD) {
	    	addMotion(&m, WALKING);
	    	//printf("Walking \t");
	    	cancelEmergencyTwo();
	    } else {
	    	addMotion(&m, RUNNING);
	    	//printf("Running \t");
	    	if (running_counter != 0xFF) {
	    		running_counter++;
	    	}
	    }

	    if (running_counter >= RUNNING_MAX) {
	    	triggerEmergencyTwo();
	    }

	    //printf("Acce: %" PRId64 "\tAvg: %" PRId64 "\n", acc, average_movement);
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
	status.systemEmergencyOne = 0;
	status.systemEmergencyTwo = 0;

	static struct etimer movementReadInterval;
	static struct etimer temperatureReadInterval;	
	static struct etimer rssiReadInterval;	

	//Initialize timers for intervals
	etimer_set(&movementReadInterval, movement_read_interval);
	etimer_set(&rssiReadInterval, rssi_read_interval);
	etimer_set(&temperatureReadInterval, temp_read_intreval);	
	
	//Initialize sensors
	SENSORS_ACTIVATE(button_sensor);
	//tmp102_init();			
		
	//Process main loop
	while(1) {

		PROCESS_WAIT_EVENT();
		
		if(etimer_expired(&movementReadInterval)){
			SENSORS_ACTIVATE(adxl345);
			readMovement();
			etimer_set(&movementReadInterval, movement_read_interval);
			SENSORS_DEACTIVATE(adxl345);
		}		
		if(etimer_expired(&temperatureReadInterval)){
			SENSORS_ACTIVATE(battery_sensor);
			SENSORS_ACTIVATE(tmp102);
			readTemperature();
			readBattery();			
			etimer_set(&temperatureReadInterval, temp_read_intreval);
			SENSORS_DEACTIVATE(battery_sensor);
			SENSORS_DEACTIVATE(tmp102);
		}		
	}
	exit:		
		SENSORS_DEACTIVATE(button_sensor);
		SENSORS_DEACTIVATE(battery_sensor);	
		SENSORS_DEACTIVATE(adxl345);
		SENSORS_DEACTIVATE(tmp102);	
		leds_off(LEDS_ALL);
		PROCESS_END();
}

/*
* Communication process
*/
PROCESS_THREAD(communication, ev, data)
{	
	//Our process		
	PROCESS_BEGIN();

	resetMessage(&m);
	printf("Communication process\n");	
	setAddress(node_id, myAddress_2);
	setCurrentGateway(defaultGateway);	

	static struct etimer ackCountInterval;
	static struct etimer sendInterval;
	static struct etimer meshRefreshInterval;
		
	etimer_set(&sendInterval, send_interval);
	etimer_set(&meshRefreshInterval, mesh_refresh_interval);
	etimer_set(&ackCountInterval, ack_count_interval);
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
			etimer_set(&sendInterval, send_interval);			
		}
		//check if we are lost :)
		if(etimer_expired(&ackCountInterval)){
			toggleEmergencyOne();
			etimer_reset(&ackCountInterval);
		}		
		//reinitialize mesh if sending failed more than 5 times
		//TODO: SendFailedCounter=queue length
		//if(sendFailedCounter%10==0){
		if(etimer_expired(&meshRefreshInterval)){
			printf("NETWORK: Closing Mesh\n");
			mesh_close(&mesh);
			m.neighbourCount = 0;
			mesh_open(&mesh, 14, &callbacks);
			printf("NETWORK: Initializing Mesh\n");			
			//sendFailedCounter+=10;
			etimer_set(&meshRefreshInterval, mesh_refresh_interval);
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

	etimer_set(&neighborAdvertismentInterval, neighbor_advertisment_interval);
	etimer_set(&neighborSenseInterval, neighbor_sense_interval);
	etimer_set(&neighborSenseTime, neighbor_sense_time);
	etimer_set(&neighborTableRinitializeInterval, neighbor_table_reinitialize_interval);
	uint8_t sensing = 0;	
	//Process main loop
	while(1) {
		
		PROCESS_WAIT_EVENT();	
		
		//sense neighbors every cca 5s for 1s
		if(etimer_expired(&neighborSenseInterval)){
			if(sensing==0){
				broadcast_open(&broadcast, 129, &broadcast_call);
				//printf("Sensing for neighbors\n");								
				etimer_set(&neighborSenseInterval, neighbor_sense_time);
				sensing = 1;
			}	
			else{
				broadcast_close(&broadcast);
				//printf("Sensing stoped\n");					
				etimer_set(&neighborSenseInterval, neighbor_sense_interval + random_rand() % (neighbor_sense_interval));
				sensing = 0;
			}														
		}
								
		//send advertisment to your neighbors every 5s
		if(etimer_expired(&neighborAdvertismentInterval)){
			broadcast_open(&broadcast, 129, &broadcast_call);
 			packetbuf_copyfrom("Hello", 5);    
    		broadcast_send(&broadcast);
    		//printf("Neighbor advertisment sent\n");
			etimer_set(&neighborAdvertismentInterval, neighbor_advertisment_interval + random_rand() % (neighbor_advertisment_interval));
			broadcast_close(&broadcast);
		}				
	}
	PROCESS_END();
}
