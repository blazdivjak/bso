#ifndef __GATEWAY_H__
#define __GATEWAY_H__


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
#include "sys/stimer.h" 
#include "../lib/libmessage.h"
#include "random.h"
#include "ringbuf.h"
#include "lib/list.h"
#include "lib/memb.h"
#include <setjmp.h>  // exceptions

// timers
#define MESH_REFRESH_INTERVAL (CLOCK_SECOND)*60*30
#define COW_MISSING_INTERVAL (CLOCK_SECOND)*10 // interval in which alarm is raised after the cow went missing
#define COWS_SEEN_COUNTER_INTERVAL (CLOCK_SECOND)*60 // interval in which every cow should be able to deliver at least one message
#define CLUSTERS_REFRESH_INTERVAL (CLOCK_SECOND)*60*6
#define CLUSTERS_RETRY_INTERVAL (CLOCK_SECOND)*60*1
#define COMMAND_SEND_INTERVAL (CLOCK_SECOND)*1
#define ERROR_RECOVERY_INTERVAL CLOCK_SECOND * 2

// uart and commands
#define UART0_CONF_WITH_INPUT 1
#define CMD_NUMBER_OF_MOTES "NOM"
#define CMD_QUERY "QRY"	// send with >>QRY 7<< to query mote id 7

// addressing
#define GATEWAY_ADDRESS 0
#define MY_ADDRESS_1 0//1
#define MY_ADDRESS_2 0//1
static uint8_t myAddress_1 = MY_ADDRESS_1;
static uint8_t myAddress_2 = MY_ADDRESS_2;
static uint8_t sendFailedCounter = 0;

// cows registered and present in network
#define MAX_NUMBER_OF_COWS 32
#define NUMBER_OF_COWS 5
static int register_cows[NUMBER_OF_COWS] = {1, 2, 3, 5, 6}; // register cow addresses
static uint32_t cows_registered = 0; // bitmap for registered cows
static uint32_t cows_missing = 0; // bitmap for missing cows
static uint8_t cows_seen_counter[NUMBER_OF_COWS]; // count how many times the cow is seen in network - if zero raise alarm
static uint32_t cows_seen_counter_status = 0;
#define COWS_SEEN_ALARM_WINDOW 2 // iterations of COWS_SEEN_COUNTER_INTERVAL to keep alarm on (default: 3)
static uint8_t cows_seen_alarm_window = COWS_SEEN_ALARM_WINDOW;

// cows sensors reading storage
static uint8_t batterys[NUMBER_OF_COWS];
static uint8_t num_of_motions[NUMBER_OF_COWS];
static uint64_t motions[NUMBER_OF_COWS];
static uint8_t num_of_neighbours[NUMBER_OF_COWS];
static uint8_t neighbours[NUMBER_OF_COWS][NUMBER_OF_COWS];
static uint8_t myhops[NUMBER_OF_COWS];
 // clusters
struct Cluster{
	uint8_t head;
	uint8_t members_count;
	uint8_t members[NUMBER_OF_COWS];
} Cluster;
#define CLUSTER_FORMULA_TRESHOLD 3
#define CLUSTER_FORMULA_WEIGHT_NUM_NEIGHBOURS 5 // values from 0 to n * WEIGHT
#define CLUSTER_FORMULA_WEIGHT_NUM_MOTION 1 // values from 0 to 2 * WEIGHT
#define CLUSTER_FORMULA_WEIGHT_NUM_HOPS 1 // values from 0 to n * WEIGHT
#define CLUSTER_FORMULA_WEIGHT_NUM_BATTERY 1 // valuse from 1 to 5 * WEIGHT
static int cluster_scores[NUMBER_OF_COWS];  // scores for each cluster candidate
static uint8_t clusters_count; // number of clusters in network
static struct Cluster clusters[NUMBER_OF_COWS];  // cluster objects

//Ringbuffer


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
static struct etimer commander_interval;
static struct etimer error_recovery_interval;

static struct ringbuf commanderBuff;
#define COMMANDER_BUFF_SIZE 64
static uint8_t commanderBuff_data[COMMANDER_BUFF_SIZE];

// status of nodes in network
struct {
  uint32_t emergency_missing;
  uint8_t emergency_missing_counter;
  uint32_t emergency_running;
} status;

/* Functions for cattle management */
static char * byte_to_binary(uint32_t x);
static int count_cows(uint32_t x);
static void cows_registration();
static int find_cow_with_id(uint8_t id);
static int find_cow_id_from_bitmap(uint32_t bitmap);

/* Command functions */
static void handleCommand(CmdMsg *command);
static void broadcast_CmdMsg(uint8_t command_id, uint8_t target, uint8_t destinationAddress);
static int readRSSI();

/* Initialize mesh */
static void sent(struct mesh_conn *c);
static void timedout(struct mesh_conn *c);
static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops);
const static struct mesh_callbacks callbacks = {recv, sent, timedout};

/* Address function */
static void setAddress(uint8_t myAddress_1, uint8_t myAddress_2);

/* Timer handlers */
static void handle_reset_mesh();
static void handle_clusters();
static void handle_missing_cows();
static void handle_cows_seen_refresh();


/* Error detection and recovery */
#define NUM_ACK_ENTRIES 64
#define NUM_OF_RETRIES 6
struct ack_entry {
  struct ack_entry *next;
  uint8_t to;
  uint8_t buffer[CMD_BUFFER_MAX_SIZE];
  uint8_t retries;
};
LIST(ack_list);
MEMB(ack_mem, struct ack_entry, NUM_ACK_ENTRIES);

void findRemoveFromAckList(uint8_t id, uint8_t mote_id);







#endif