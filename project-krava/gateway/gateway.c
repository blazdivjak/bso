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
#include <setjmp.h>  // exceptions

#define UART0_CONF_WITH_INPUT 1
#define CMD_NUMBER_OF_MOTES "NOM"
#define MESH_REFRESH_INTERVAL (CLOCK_SECOND)*60
#define GATEWAY_ADDRESS 0
#define MY_ADDRESS_1 0//1
#define MY_ADDRESS_2 0//1
#define MAX_NUMBER_OF_COWS 32
#define NUMBER_OF_COWS 3
#define COW_MISSING_INTERVAL (CLOCK_SECOND)*30
// exceptions
#define TRY do{ jmp_buf ex_buf__; if( !setjmp(ex_buf__) ){
#define CATCH } else {
#define ETRY } }while(0)
#define THROW longjmp(ex_buf__, 1)

/*
Static values
*/
static uint8_t myAddress_1 = MY_ADDRESS_1;
static uint8_t myAddress_2 = MY_ADDRESS_2;
static uint8_t sendFailedCounter = 0;
static uint32_t cows_registered = 0;
static uint32_t cows_in_range = 0;
static uint32_t cows_missing = 0;
static int register_cows[NUMBER_OF_COWS] = {1, 2, 3};

/*
 * Functions for cattle management
 */
// converts uint32_t to string where number is represented in binary
static char * byte_to_binary(uint32_t x) {
  static char b[33];
  b[0] = '\0';
  uint32_t z;
  for (z = 0x80000000; z > 0; z >>= 1) {
    strcat(b, ((x & z) == z) ? "1" : "0");
  }
  return b;
}

// count number of booleans activated in number
static int count_cows(uint32_t x){
  int c = 0;
  uint32_t z;
  for (z = 0x80000000; z > 0; z >>= 1) {
    c += ((x & z) == z) ? 1 : 0;
  }
  return c;
}

// register cows into cows_registered number
static uint32_t cows_registration() {
  uint32_t r = 0;
  int i;
  for (i = 0; i < NUMBER_OF_COWS; i ++){
    r |= 1 << register_cows[i];
  }
  return r;
}

// check if any of the cows went out of range
static void cows_find_missing() {
  cows_missing = cows_registered ^ cows_in_range;
}

/*
* Initialize mesh
*/
static struct mesh_conn mesh;

static void sent(struct mesh_conn *c) {
  //printf("Packet sent\n");
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
  packetbuf_copyfrom(&m.id, 1);
  mesh_send(&mesh, from);

  // save how many cows are in range
  cows_in_range |= 1 << m.mote_id;
  cows_missing = cows_registered ^ cows_in_range;
  //printf("Cows registered with gateway: %s\n", byte_to_binary(cows_in_range));

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
  /* Register cows into bitmap */
  cows_registered = cows_registration();
  /* Set timer to refresh the missing cow interval*/
  static struct etimer cows_missing_interval;
  etimer_set(&cows_missing_interval, COW_MISSING_INTERVAL);

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
    if(etimer_expired(&cows_missing_interval)){
      cows_find_missing();
      etimer_reset(&cows_missing_interval);
    }

    if (ev == serial_line_event_message && data != NULL) {
    	printf("received line: %s\n", (char *)data);
      if (!strcmp(CMD_NUMBER_OF_MOTES, data)) {
        char creg[33], cmis[33], cran[33];
        char * t = byte_to_binary(cows_registered);
        strcpy(creg, t);
        t = byte_to_binary(cows_missing);
        strcpy(cmis, t);
        t = byte_to_binary(cows_in_range);
        strcpy(cran, t);
        printf("Counting cows:\n    %2d %16s %s \n    %2d %16s %s \n    %2d %16s %s \n",
          count_cows(cows_registered),
          "cows registered",
          creg,
          count_cows(cows_in_range),
          "cows in range",
          cran,
          count_cows(cows_missing),
          "cows missing",
          cmis
        );
      }
    }

  }
  PROCESS_END();
}
