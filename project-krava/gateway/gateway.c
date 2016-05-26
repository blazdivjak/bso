/*
*__authors: blaz, gregor, gasper__
*__date: 2016-04-07__
*__assigment1__
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "contiki.h"
#include "dev/serial-line.h"
#include "dev/uart0.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/i2cmaster.h"  // Include IC driver
#include "dev/adxl345.h"
#include "net/rime/rime.h"
#include "net/rime/mesh.h"
#include "sys/node-id.h" 
#include "../lib/libmessage.h"
#include "random.h"

#define UART0_CONF_WITH_INPUT 1
#define CMD_NUMBER_OF_MOTES "NOM"
#define MESH_REFRESH_INTERVAL (CLOCK_SECOND)*60
#define GATEWAY_ADDRESS 0
#define MY_ADDRESS_1 0//1
#define MY_ADDRESS_2 0//1

/*
Static values
*/
static uint8_t myAddress_1 = MY_ADDRESS_1;
static uint8_t myAddress_2 = MY_ADDRESS_2;
static uint8_t sendFailedCounter = 0;

/*
* Initialize mesh
*/
static struct mesh_conn mesh;

static void sent(struct mesh_conn *c) {
  printf("Packet sent\n");
}

static void timedout(struct mesh_conn *c) {
  sendFailedCounter += 1;
  printf("packet timedout. Failed to send packet counter: %d\n", sendFailedCounter);
}

static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops){
  printf("Data received from %d.%d: %d bytes\n",from->u8[0], from->u8[1], packetbuf_datalen());
	
  // if sent to me? and the message format is right ...
  Message m;
  decode(packetbuf_dataptr(), packetbuf_datalen(), &m);
  printMessage(&m);
  
  //uint16_t message_id = m->id;

  packetbuf_copyfrom(m.id, 2);
  mesh_send(&mesh, from);
}
const static struct mesh_callbacks callbacks = {recv, sent, timedout};

/*
 * Address function
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

/*
 * Init process
 */
PROCESS(gateway_main, "Main gateway proces");
AUTOSTART_PROCESSES(&gateway_main);

static struct etimer et;
PROCESS_THREAD(gateway_main, ev, data)
{  
  PROCESS_EXITHANDLER(mesh_close(&mesh);)
  PROCESS_BEGIN();  
  /* Init UART for receiveing */
  uart0_init(BAUD2UBR(115200));
  uart0_set_input(serial_line_input_byte);

  /* Set gateway address */
  setAddress(myAddress_1, myAddress_2);

  /* Start mesh */
  static struct etimer meshRefreshInterval;
  etimer_set(&meshRefreshInterval, MESH_REFRESH_INTERVAL);
  mesh_open(&mesh, 14, &callbacks);

  while(1) {    

    PROCESS_WAIT_EVENT(); 
    
    if(etimer_expired(&meshRefreshInterval)){
      printf("Closing Mesh\n");
      mesh_close(&mesh);
      mesh_open(&mesh, 14, &callbacks);
      printf("Initializing Mesh\n");      
      //sendFailedCounter+=10;
      etimer_reset(&meshRefreshInterval);
    }

    if (ev == serial_line_event_message && data != NULL) {
    	printf("received line: %s\n", (char *)data);
      if (!strcmp(CMD_NUMBER_OF_MOTES, data)) {
        printf("get number of motes..\n");
      }
    }

  }
  PROCESS_END();
}
