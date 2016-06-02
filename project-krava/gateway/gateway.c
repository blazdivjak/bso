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
#define COW_MISSING_INTERVAL (CLOCK_SECOND)*10
// exceptions
#define TRY do{ jmp_buf ex_buf__; if( !setjmp(ex_buf__) ){
#define CATCH } else {
#define ETRY } }while(0)
#define THROW longjmp(ex_buf__, 1)
// clustering
#define CLUSTERS_REFRESH_INTERVAL (CLOCK_SECOND)*40
#define RSSI_TRESHOLD -75
// cows seen refresh interval in which every cow should be able to deliver at least one message
#define COWS_SEEN_COUNTER_REFRESH_INTERVAL (CLOCK_SECOND)*60

/*
Static values
*/
static uint8_t myAddress_1 = MY_ADDRESS_1;
static uint8_t myAddress_2 = MY_ADDRESS_2;
static uint8_t sendFailedCounter = 0;
// cows registered and present in network
static uint32_t cows_registered = 0;
static uint32_t cows_in_range = 0;
static uint32_t cows_missing = 0;
static int register_cows[NUMBER_OF_COWS] = {1, 2, 3};
// cows seen counter - count how many times the cow is seen in network - if zero raise alarm
static uint8_t cows_seen_counter[MAX_NUMBER_OF_COWS];
static uint32_t cows_seen_counter_status = 0;
// cows sensors reading storage
static uint8_t batterys[NUMBER_OF_COWS];
static uint8_t motions[NUMBER_OF_COWS];
static uint8_t num_of_neighbours[NUMBER_OF_COWS];
static uint8_t neighbours[NUMBER_OF_COWS][MAX_NUMBER_OF_COWS];
// clusters
static uint8_t cluster_counts[MAX_NUMBER_OF_COWS];  //  number of cows in cluster
static uint32_t clusters[MAX_NUMBER_OF_COWS];  // cluster ids

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
static void find_missing_cows() {
  cows_missing = cows_registered ^ cows_in_range;
}

void handleCommand(CmdMsg *command) {
  
  if (command->cmd == CMD_EMERGENCY_ONE) {
    printf("Emergency one, cow unreachable id: %d\n", command->target_id);
  } else if (command->cmd == CMD_EMERGENCY_TWO) {
    printf("Emergency two, cow running id: %d\n", command->target_id);
  } else if (command->cmd == CMD_CANCEL_EMERGENCY_ONE) {
    printf("Emergency one cancel, cow id: %d\n", command->target_id);
  } else if (command->cmd == CMD_CANCEL_EMERGENCY_TWO) {
    printf("Emergency two cancel, cow id: %d\n", command->target_id);
  } 
}

int readRSSI() {   
  return packetbuf_attr(PACKETBUF_ATTR_RSSI) - 45;
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
	

  // ACK
  if(packetbuf_datalen()==1){
    printf("Message ID: %d ACK received.\n", ((uint8_t *)packetbuf_dataptr())[0]);
    //ackMessage(&myPackets, ((uint8_t *)packetbuf_dataptr())[0]);
  }
  // Krava message
  else if((((uint8_t *)packetbuf_dataptr())[0] & 0x01) == 0){
    // TODO: if sent to me? and the message format is right ...

    // read packet RSSI value
    if (hops <= 1) {
      // This packet is from neighbouring krava, read and save the RSSI value
      int rssi; 
      rssi = readRSSI();
    }

    // message decode
    Message m;
    decode(packetbuf_dataptr(), packetbuf_datalen(), &m);
    printMessage(&m);
    packetbuf_copyfrom(&m.id, 1);
    mesh_send(&mesh, from);

    // update cows that are in range
    cows_in_range |= 1 << m.mote_id;
    find_missing_cows();
    // update info on how many times the cow is seen
    cows_seen_counter[m.mote_id] += 1;
    
    // update status of sensors for each cow
    motions[m.mote_id] = m.motions;
    batterys[m.mote_id] = m.battery;
    num_of_neighbours[m.mote_id] = m.neighbourCount;
    int i;
    for (i = 0; i < m.neighbourCount; i++){
      neighbours[m.mote_id][i] = m.neighbours[i];
    }
  }
  // Command
  else{    
    CmdMsg command;
    decodeCmdMsg(packetbuf_dataptr(), &command);
    handleCommand(&command);
  }  

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

  /* Set mesh refresh timer */
  static struct etimer meshRefreshInterval;
  etimer_set(&meshRefreshInterval, MESH_REFRESH_INTERVAL);
  mesh_open(&mesh, 14, &callbacks);

  /* Set timer for cluster generation */
  static struct etimer clusters_refresh_interval;
  etimer_set(&clusters_refresh_interval, CLUSTERS_REFRESH_INTERVAL);

  /* Set timer for cluster generation */
  static struct etimer cows_seen_counter_refresh_interval;
  etimer_set(&cows_seen_counter_refresh_interval, COWS_SEEN_COUNTER_REFRESH_INTERVAL);

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
      find_missing_cows();
      etimer_reset(&cows_missing_interval);
    }
    if(etimer_expired(&clusters_refresh_interval)){
      printf("Timer for cluster refresh expired\n");
      etimer_reset(&clusters_refresh_interval);
    }
    if(etimer_expired(&cows_seen_counter_refresh_interval)){
      cows_seen_counter_status = 0;
      int i = 0;
      for (i=0; i < MAX_NUMBER_OF_COWS; i++) {
        if (cows_seen_counter[i] > 0) {
          cows_seen_counter_status |= 1 << i;
        }
        cows_seen_counter[i] = 0;
      }
      printf("Missing %d registered cows: %s\n", 
        count_cows(cows_seen_counter_status) - count_cows(cows_registered), 
        byte_to_binary(cows_seen_counter_status ^ cows_registered));
      etimer_reset(&cows_seen_counter_refresh_interval);
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
