/*
 * __authors: blaz, gregor, gasper__
 * __date: 2016-04-07__
 * __assigment1__
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

// timers
#define MESH_REFRESH_INTERVAL (CLOCK_SECOND)*60
#define COW_MISSING_INTERVAL (CLOCK_SECOND)*10 // interval in which alarm is raised after the cow went missing
#define COWS_SEEN_COUNTER_INTERVAL (CLOCK_SECOND)*60 // interval in which every cow should be able to deliver at least one message
#define CLUSTERS_REFRESH_INTERVAL (CLOCK_SECOND)*40

// uart and commands
#define UART0_CONF_WITH_INPUT 1
#define CMD_NUMBER_OF_MOTES "NOM"

// addressing
#define GATEWAY_ADDRESS 0
#define MY_ADDRESS_1 0//1
#define MY_ADDRESS_2 0//1
static uint8_t myAddress_1 = MY_ADDRESS_1;
static uint8_t myAddress_2 = MY_ADDRESS_2;
static uint8_t sendFailedCounter = 0;

// cows registered and present in network
#define MAX_NUMBER_OF_COWS 32
#define NUMBER_OF_COWS 3
static int register_cows[NUMBER_OF_COWS] = {1, 2, 3}; // register cow addresses
static uint32_t cows_registered = 0; // bitmap for registered cows
static uint32_t cows_missing = 0; // bitmap for missing cows
static uint8_t cows_seen_counter[NUMBER_OF_COWS]; // count how many times the cow is seen in network - if zero raise alarm
static uint32_t cows_seen_counter_status = 0;
#define COWS_SEEN_ALARM_WINDOW 3 // iterations of COWS_SEEN_COUNTER_INTERVAL to keep alarm on (default: 3)
static uint8_t cows_seen_alarm_window = COWS_SEEN_ALARM_WINDOW;

// cows sensors reading storage
static uint8_t batterys[NUMBER_OF_COWS];
static uint8_t motions[NUMBER_OF_COWS];
static uint8_t num_of_neighbours[NUMBER_OF_COWS];
static uint8_t neighbours[NUMBER_OF_COWS][NUMBER_OF_COWS];

// clusters
static uint8_t cluster_counts[NUMBER_OF_COWS];  // number of cows in cluster
static uint32_t clusters[NUMBER_OF_COWS];  // cluster ids

// mesh
static struct mesh_conn mesh;

// command message
static uint8_t command_buffer[CMD_BUFFER_MAX_SIZE];
static struct CmdMsg command;

//// RSSI
//#define RSSI_TRESHOLD -75

// timers
static struct etimer cows_missing_interval;
static struct etimer meshRefreshInterval;
static struct etimer clusters_refresh_interval;
static struct etimer cows_seen_counter_refresh_interval;

// status of nodes in network
struct {
  uint32_t emergency_missing;
  uint8_t emergency_missing_counter;
  uint32_t emergency_running;
} status;

/* Functions for cattle management */
static char * byte_to_binary(uint32_t x) {
  // converts uint32_t to string where number is represented in binary
  static char b[33];
  b[0] = '\0';
  uint32_t z;
  for (z = 0x80000000; z > 0; z >>= 1) {
    strcat(b, ((x & z) == z) ? "1" : "0");
  }
  return b;
}

static int count_cows(uint32_t x){
  // count number of booleans activated in number
  int c = 0;
  uint32_t z;
  for (z = 0x80000000; z > 0; z >>= 1) {
    c += ((x & z) == z) ? 1 : 0;
  }
  return c;
}

static void cows_registration() {
  // register cows into cows_registered bitmap
  cows_registered = 0;
  int i;
  for (i = 0; i < NUMBER_OF_COWS; i ++){
    cows_registered |= 1 << i;
  }
}

static int find_cow_with_id(uint8_t id) {
  int i;
  for (i = 0; i < NUMBER_OF_COWS; i++) {
    if (register_cows[i] == id) {
      break;
    }
  }
  return i;
}

static int find_cow_id_from_bitmap(uint32_t bitmap) {
  uint8_t i;
  for (i = 0; i < MAX_NUMBER_OF_COWS; i ++) {
    if ((bitmap & 1 << i) > 0) {
      break;
    }
  }
  return find_cow_with_id(i);
}

/* Command functions */
static void handleCommand(CmdMsg *command) {
  
  printf("COMMAND: Processing command\n");

  if (command->cmd == CMD_EMERGENCY_ONE) {
    printf("COMMAND: Emergency one, cow unreachable id: %d\n", command->target_id);
  } else if (command->cmd == CMD_EMERGENCY_TWO) {
    printf("COMMAND: Emergency two, cow running id: %d\n", command->target_id);
  } else if (command->cmd == CMD_CANCEL_EMERGENCY_ONE) {
    printf("COMMAND: Emergency one cancel, cow id: %d\n", command->target_id);
  } else if (command->cmd == CMD_CANCEL_EMERGENCY_TWO) {
    printf("COMMAND: Emergency two cancel, cow id: %d\n", command->target_id);
  } 
}

static void broadcast_CmdMsg(int command_id, int target) {
  printf("COMMAND: Broadcasting command %d\n", command_id);
  setCmdMsgId(&command, 31);
  command.cmd = command_id;
  command.target_id = target;
  linkaddr_t addr;
  addr.u8[0] = myAddress_1;
  addr.u8[1] = myAddress_2;
  int i;
  for (i = 0; i < NUMBER_OF_COWS; i++) {
    addr.u8[0] = register_cows[i];
    encodeCmdMsg(&command, &command_buffer);
    packetbuf_copyfrom(command_buffer, CMD_BUFFER_MAX_SIZE);
    mesh_send(&mesh, &addr);
  }
}

static int readRSSI() {
  // read RSSI function - to be removed from GW ?
  return packetbuf_attr(PACKETBUF_ATTR_RSSI) - 45;
}

/* Initialize mesh */
static void sent(struct mesh_conn *c) {
  //printf("Packet sent\n");
}

static void timedout(struct mesh_conn *c) {
  sendFailedCounter += 1;
  printf("MESSAGES: packet timedout. Failed to send packet counter: %d\n", sendFailedCounter);
}

static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops){
  printf("MESSAGES: Data received from %d.%d: %d bytes\n",from->u8[0], from->u8[1], packetbuf_datalen());
	

  // ACK
  if(packetbuf_datalen()==1){
    printf("MESSAGES: Message ID: %d ACK received.\n", ((uint8_t *)packetbuf_dataptr())[0]);
    //ackMessage(&myPackets, ((uint8_t *)packetbuf_dataptr())[0]);
  }
  // Krava message
  else if((((uint8_t *)packetbuf_dataptr())[0] & 0x01) == 0){
    // TODO: if sent to me? and the message format is right ...

    // read packet RSSI value
    //if (hops <= 1) {
    //  // This packet is from neighbouring krava, read and save the RSSI value
    //  int rssi; 
    //  rssi = readRSSI();
    //}

    // message decode
    Message m;
    decode(packetbuf_dataptr(), packetbuf_datalen(), &m);
    printMessage(&m);
    packetbuf_copyfrom(&m.id, 1);
    mesh_send(&mesh, from); // send ACK

    // find cows index in data structures
    int cow_index = find_cow_with_id(m.mote_id);

    // update info on how many times the cow is seen
    cows_seen_counter[cow_index] += 1;
    cows_seen_counter_status |= 1 << cow_index;
    
    // update status of sensors for each cow
    motions[cow_index] = m.motions;
    batterys[cow_index] = m.battery;
    num_of_neighbours[cow_index] = m.neighbourCount;
    int i;
    for (i = 0; i < m.neighbourCount; i++){
      neighbours[cow_index][i] = m.neighbours[i];
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

/* Address function */
static void setAddress(uint8_t myAddress_1, uint8_t myAddress_2){  
  linkaddr_t addr;
  addr.u8[0] = myAddress_1;
  addr.u8[1] = myAddress_2;
  printf("My Address: %d.%d\n", addr.u8[0],addr.u8[1]);
  uint16_t shortaddr = (addr.u8[0] << 8) + addr.u8[1];
  cc2420_set_pan_addr(IEEE802154_PANID, shortaddr, NULL);   
  linkaddr_set_node_addr (&addr); 
}

/* Timer handlers */
static void handle_reset_mesh() {
  printf("NETWORK: Reinitializing Mesh\n");
  mesh_close(&mesh);
  mesh_open(&mesh, 14, &callbacks);
}

static void handle_clusters(){
  // TODO: refresh clusters
  printf("CLUSTERS: Timer for cluster refresh expired\n");
}

static void handle_missing_cows() {
  cows_missing = cows_seen_counter_status ^ cows_registered;
  uint32_t old_emergency_missing = status.emergency_missing;

  printf("CATTLE: Cows missing: %s\n", byte_to_binary(cows_missing));
  int target = find_cow_id_from_bitmap(cows_missing);
  if (count_cows(cows_missing) > 0) {
    // raise alarm two
    status.emergency_missing |= cows_missing;
  } else {
    // disable alarm one - all cows are found
    status.emergency_missing = 0;
    cows_seen_alarm_window = COWS_SEEN_ALARM_WINDOW;
    //if (old_emergency_missing != 0) { // only send cancel if previous command was alarm
    broadcast_CmdMsg(CMD_CANCEL_EMERGENCY_ONE, target);
  }

  // disable alarm if its on for more than cows seen alarm window
  if (status.emergency_missing > 0) {
    cows_seen_alarm_window -= 1;
    if (cows_seen_alarm_window <= 0) { // disable alarm after cows_seen_alarm_window iterations of alarm
      status.emergency_missing = 0;
      cows_seen_alarm_window = COWS_SEEN_ALARM_WINDOW;
      broadcast_CmdMsg(CMD_CANCEL_EMERGENCY_ONE, target);
    }
  }

  if (status.emergency_missing > 0) {
    broadcast_CmdMsg(CMD_EMERGENCY_ONE, target);
  }
}

static void handle_cows_seen_refresh() {
  cows_seen_counter_status = 0;
  int i = 0;
  for (i=0; i < NUMBER_OF_COWS; i++) {
    if (cows_seen_counter[i] > 0) {
      cows_seen_counter_status |= 1 << i;
    }
    cows_seen_counter[i] = 0;
  }
  printf("CATTLE: Missing %d registered cows: %s\n", 
    count_cows(cows_registered) - count_cows(cows_seen_counter_status), 
    byte_to_binary(cows_seen_counter_status ^ cows_registered));
}

/* Init process */
PROCESS(gateway_main, "Main gateway proces");
AUTOSTART_PROCESSES(&gateway_main);

PROCESS_THREAD(gateway_main, ev, data)
{  
  PROCESS_EXITHANDLER(mesh_close(&mesh);)
  PROCESS_BEGIN();  
  
  /* Init functions */
  uart0_init(BAUD2UBR(115200));
  uart0_set_input(serial_line_input_byte);
  setAddress(myAddress_1, myAddress_2);
  cows_registration();

  /* Set timers */
  etimer_set(&cows_missing_interval, COW_MISSING_INTERVAL);
  etimer_set(&meshRefreshInterval, MESH_REFRESH_INTERVAL);
  etimer_set(&clusters_refresh_interval, CLUSTERS_REFRESH_INTERVAL);
  etimer_set(&cows_seen_counter_refresh_interval, COWS_SEEN_COUNTER_INTERVAL);

  /* set mesh channel */
  mesh_open(&mesh, 14, &callbacks);

  while(1) {

    PROCESS_WAIT_EVENT(); 
    
    if(etimer_expired(&meshRefreshInterval)){
      handle_reset_mesh();
      etimer_reset(&meshRefreshInterval);
    }

    if(etimer_expired(&cows_missing_interval)){
      handle_missing_cows();
      etimer_reset(&cows_missing_interval);
    }

    if(etimer_expired(&clusters_refresh_interval)){
      handle_clusters();
      etimer_reset(&clusters_refresh_interval);
    }

    if(etimer_expired(&cows_seen_counter_refresh_interval)){
      handle_cows_seen_refresh();
      etimer_reset(&cows_seen_counter_refresh_interval);
    }

    if (ev == serial_line_event_message && data != NULL) {
    	printf("SERIAL: received line: %s\n", (char *)data);
      if (!strcmp(CMD_NUMBER_OF_MOTES, data)) {
        char creg[33], cmis[33];
        char * t = byte_to_binary(cows_registered);
        strcpy(creg, t);
        t = byte_to_binary(cows_missing);
        strcpy(cmis, t);
        printf("SERIAL: Counting cows:\nSERIAL:  %2d %16s %s \nSERIAL:  %2d %16s %s \n",
          count_cows(cows_registered),
          "cows registered",
          creg,
          count_cows(cows_missing),
          "cows missing",
          cmis
        );
      }
    }

  }
  PROCESS_END();
}
