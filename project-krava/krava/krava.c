/*
*__authors: blaz, gregor, gasper__
*__date: 2016-04-15__
*__project krava__
*/
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

#define MOVEMENT_READ_INTERVAL (CLOCK_SECOND)*1
#define RSSI_READ_INTERVAL (CLOCK_SECOND)*5
#define MESH_REFRESH_INTERVAL (CLOCK_SECOND)*360
#define TEMP_READ_INTERVAL (CLOCK_SECOND)*30
#define BATTERY_READ_INTERVAL (CLOCK_SECOND)*30
#define SEND_INTERVAL (CLOCK_SECOND)*30

#define GATEWAY_ADDRESS 0
#define MY_ADDRESS_1 0//1
#define MY_ADDRESS_2 0//1

static uint8_t myAddress_1 = MY_ADDRESS_1;
static uint8_t myAddress_2 = MY_ADDRESS_2;
static uint8_t sendFailedCounter = 0;

#define NEIGHBOR_TABLE_REINITIALIZE_INTERVAL (CLOCK_SECOND)*360
#define NEIGHBOR_SENSE_INTERVAL (CLOCK_SECOND)*5
#define NEIGHBOR_SENSE_TIME (CLOCK_SECOND)*1
#define NEIGHBOR_ADVERTISEMENT_INTERVAL (CLOCK_SECOND)*5
static uint8_t kravaNeighbors[20];
static uint8_t numberOfNeighbors = 0;

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

  //TODO: Check received packet type if it is ACK remove packet from SENT PACKET QUEUE
  		 
  printf("Data received from %d.%d: %d bytes\n",from->u8[0], from->u8[1], packetbuf_datalen());
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
  linkaddr_t addr;
  addr.u8[0] = myAddress_1;
  addr.u8[1] = myAddress_2;
  printf("My Address: %d.%d\n", addr.u8[0],addr.u8[1]);
  uint16_t shortaddr = (addr.u8[0] << 8) + addr.u8[1];
  cc2420_set_pan_addr(IEEE802154_PANID, shortaddr, NULL);   
  linkaddr_set_node_addr (&addr);	
}

static void sendMessage(){
  
  printf("Sending message to my current gateway ID: %d.0\n", GATEWAY_ADDRESS);

  linkaddr_t addr_send; 
  char msg[2] = {1, 2};
  packetbuf_copyfrom(msg, 2);       
  addr_send.u8[0] = GATEWAY_ADDRESS;
  addr_send.u8[1] = 0;
  //printf("Mesh status: %d\n", mesh_ready(&mesh));
  mesh_send(&mesh, &addr_send);
}

/*
* Initialize broadcast connection
*/

static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{	

  static signed char rss;
  static signed char rss_val;
  static signed char rss_offset;	
  rss_val = packetbuf_attr(PACKETBUF_ATTR_RSSI);
  rss_offset=-45;
  rss=rss_val + rss_offset;  

  printf("Neighbor advertisment received from %d.%d: '%s' RSSI: %d\n",
         from->u8[0], from->u8[1], (char *)packetbuf_dataptr(), rss);

  //Add neighbor to current neighbor table if neighbor not in the table yet
  int addNeighborToTable = 1;
  int i;

  //TODO: Check message rssi for adding only close neighbors
  for(i = 0; i<sizeof(kravaNeighbors);i++){
  	if(kravaNeighbors[i]==from->u8[0]){
  		addNeighborToTable = 0;
  		break;
  	}
  }
  if(addNeighborToTable == 1){
  	kravaNeighbors[numberOfNeighbors%20]=from->u8[0];
  	numberOfNeighbors+=1;
  	printf("Adding neighbor: %d to table. Current number of neighbors: %d.\n",from->u8[0], numberOfNeighbors);
  }  
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;

/*
/* Sensor functions															 
*/
void readBattery(){
  
    uint16_t bateria = battery_sensor.value(0);
    float mv = (bateria * 2.500 * 2) / 4096;
    printf("Battery: %i (%ld.%03d mV)\n", bateria, (long)mv,
           (unsigned)((mv - myFloor(mv)) * 1000));
}  
void readTemperature(){
	
	static int16_t decoded_temperature;
	decoded_temperature =  decodeTemperature(tmp102_read_temp_raw());	
	printTemperature(decoded_temperature);
}
void readMovement(){
	
	int16_t x, y, z;	

	x = adxl345.value(X_AXIS);
    y = adxl345.value(Y_AXIS);
    z = adxl345.value(Z_AXIS);       
    printf("Movement: x: %d y: %d z: %d\n",x, y, z);    
}
void readRSSI(){

	printf("Reading RSSI...\n");
	//printf("last_rssi %d\n",(signed short)packetbuf_attr(PACKETBUF_ATTR_RSSI));
}


/*
*Process definitions
*/
PROCESS(krava, "Krava");
PROCESS(communication, "Communication");
PROCESS(neighbors, "Sense neigbors");
//AUTOSTART_PROCESSES(&krava, &communication, &neighbors);
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
		if(etimer_expired(&rssiReadInterval)){
			readRSSI();
			etimer_reset(&rssiReadInterval);
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
	printf("Communication process\n");
	setAddress(node_id, myAddress_2);
		
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

		//Reinitialize neighbor table TODO: Could be moved to mesh reinitialization and done there
		if(etimer_expired(&neighborTableRinitializeInterval)){
			printf("Clearing neighbor cache\n");
			etimer_reset(&neighborTableRinitializeInterval);
		}

		//sense neighbors every cca 5s for 1s
		if(etimer_expired(&neighborSenseInterval)){
			if(sensing==0){
				broadcast_open(&broadcast, 129, &broadcast_call);
				printf("Sensing for neighbors\n");								
				etimer_set(&neighborSenseTime, NEIGHBOR_SENSE_TIME);
				sensing = 1;
			}						
		}
		//Stop sensing after 1s
		if(etimer_expired(&neighborSenseTime)){
			if(sensing == 1){
				broadcast_close(&broadcast);
				printf("Sensing stoped\n");					
				etimer_set(&neighborSenseInterval, NEIGHBOR_SENSE_INTERVAL + random_rand() % (NEIGHBOR_SENSE_INTERVAL));
				sensing = 0;
			}						
		}								
			
		//send advertisment to your neighbors every 5s
		if(etimer_expired(&neighborAdvertismentInterval)){
			broadcast_open(&broadcast, 129, &broadcast_call);
 			packetbuf_copyfrom("Hello I am your neighbor krava", 30);    
    		broadcast_send(&broadcast);
    		printf("Neighbor advertisment sent\n");
			etimer_set(&neighborAdvertismentInterval, NEIGHBOR_SENSE_INTERVAL + random_rand() % (NEIGHBOR_SENSE_INTERVAL));
			broadcast_close(&broadcast);
		}				
	}
	PROCESS_END();
}
