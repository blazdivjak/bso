/*
*__author: blaz, gregor, gasper__
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
#include "net/rime/rime.h"
#include "net/rime/mesh.h"
#include "sys/node-id.h" 

#include "../lib/libmessage.h"

#define UART0_CONF_WITH_INPUT 1

// Commands
#define CMD_NUMBER_OF_MOTES "NOM"

/*
Static values
*/
static uint8_t myAddress = 0;
static uint16_t mote_addrs[] = {0x0001};

static struct mesh_conn mesh;



PROCESS(gateway_main, "Main gateway proces");
AUTOSTART_PROCESSES(&gateway_main);

static void increase_address(){  
  linkaddr_t addr;  
  addr.u8[0] = myAddress;
  addr.u8[1] = 0;
  printf("My Address: %d.%d\n", addr.u8[0],addr.u8[1]);
  uint16_t shortaddr = (addr.u8[0] << 8) + addr.u8[1];
  cc2420_set_pan_addr(IEEE802154_PANID, shortaddr, NULL);   
  linkaddr_set_node_addr (&addr);
  myAddress+=1;
}


static void sent(struct mesh_conn *c)
{
  printf("Packet sent\n");
}

static void timedout(struct mesh_conn *c)
{
  printf("Packet timedout\n");
}

static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops){
  printf("Data received from %d.%d: %d bytes\n",from->u8[0], from->u8[1], packetbuf_datalen());
	// TODO
  struct Message msg = decode((char *)packetbuf_dataptr());
  printMessage(msg);
}

/*---------------------------------------------------------------------------*/
// callbacks for mesh (must be declared after declaration)
const static struct mesh_callbacks callbacks = {recv, sent, timedout};
/*---------------------------------------------------------------------------*/

static struct etimer et;
PROCESS_THREAD(gateway_main, ev, data)
{  
  PROCESS_EXITHANDLER(mesh_close(&mesh);)
  PROCESS_BEGIN();  
  /* Init UART for receiveing */
  uart0_init(BAUD2UBR(115200));
  uart0_set_input(serial_line_input_byte);

  mesh_open(&mesh, 14, &callbacks);  
  
  SENSORS_ACTIVATE(button_sensor);

  // set address
  increase_address();
  // myAddress = node_id;

  while(1) {    

    PROCESS_WAIT_EVENT(); 
    //set address
    if(ev == sensors_event && data == &button_sensor){
    	printf("button\n");
      increase_address();
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