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
//#include "../lib/libmath.h"

#define MOVEMENT_READ_INTERVAL (CLOCK_SECOND)*1
#define RSSI_READ_INTERVAL (CLOCK_SECOND)*5
#define MESH_REFRESH_INTERVAL (CLOCK_SECOND)*60
#define TEMP_READ_INTERVAL (CLOCK_SECOND)*30
#define BATTERY_READ_INTERVAL (CLOCK_SECOND)*30
#define SEND_INTERVAL (CLOCK_SECOND)*30

#define GATEWAY_ADDRESS 0
#define MY_ADDRESS 1

static uint8_t myAddress = MY_ADDRESS;

/*
* Mesh functions
*/

static void sent(struct mesh_conn *c)
{
  printf("packet sent\n");
}
static void timedout(struct mesh_conn *c)
{
  printf("packet timedout\n");
}

static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops){
  
  printf("Data received from %d.%d: %d bytes\n",from->u8[0], from->u8[1], packetbuf_datalen());

  /*if (myAddress == FROM_MOTE_ADDRESS){    
    // receive ACK and clear queue
    queue_remove_first(message_size/3);

  } else if (myAddress == TO_MOTE_ADDRESS){
    // receive data, print to STD_OUT
    uint8_t index_to_confirm;
    int i;
    for(i=0; i<packetbuf_datalen(); i+=3){
      index_to_confirm = decode_temp(((char *)packetbuf_dataptr()) + i);
    }
    //confirm message a.k.a send ACK
    packetbuf_copyfrom(index_to_confirm, 1);
    mesh_send(&mesh, from);
  }  */
}

/*
* Initialize mesh
*/
static struct mesh_conn mesh;
const static struct mesh_callbacks callbacks = {recv, sent, timedout};

/*
* Communication functions
*/
static void setAddress(){  
  linkaddr_t addr;  
  addr.u8[0] = myAddress;
  addr.u8[1] = 1;
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
AUTOSTART_PROCESSES(&krava, &communication);

/*
* Krava process
*/
PROCESS_THREAD(krava, ev, data)
{	
	//Our process
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

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
	setAddress();
	
	static struct etimer meshRefreshInterval;
	static struct etimer sendInterval;

	etimer_set(&meshRefreshInterval, MESH_REFRESH_INTERVAL);
	etimer_set(&sendInterval, SEND_INTERVAL);

	//Process main loop
	while(1) {
		mesh_open(&mesh, 14, &callbacks);
	 
		PROCESS_WAIT_EVENT();
		if(etimer_expired(&sendInterval)){
			sendMessage();
			etimer_reset(&sendInterval);
		}
		if(etimer_expired(&meshRefreshInterval)){
			mesh_close(&mesh);
			etimer_reset(&meshRefreshInterval);
		}
	}	
	PROCESS_END();
}
