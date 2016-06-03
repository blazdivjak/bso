/*
 * __authors: blaz, gregor, gasper__
 * __date: 2016-04-07__
 * __assigment1__
 */
#include "gateway.h"

#define DEBUG 1

#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

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

static int count_cows(uint32_t x) {
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
  
  PRINTF("COMMAND: Processing command\n");

  if (command->cmd == CMD_EMERGENCY_ONE) {
    printf("COMMAND: Emergency one, cow unreachable id: %d\n", command->target_id);

  } else if (command->cmd == CMD_EMERGENCY_TWO) {
    printf("COMMAND: Emergency two, cow running id: %d\n", command->target_id);
    broadcast_CmdMsg(CMD_EMERGENCY_TWO, command->target_id);

  } else if (command->cmd == CMD_CANCEL_EMERGENCY_ONE) {
    printf("COMMAND: Emergency one cancel, cow id: %d\n", command->target_id);
    broadcast_CmdMsg(CMD_CANCEL_EMERGENCY_ONE, command->target_id);

  } else if (command->cmd == CMD_CANCEL_EMERGENCY_TWO) {
    printf("COMMAND: Emergency two cancel, cow id: %d\n", command->target_id);
    broadcast_CmdMsg(CMD_CANCEL_EMERGENCY_TWO, command->target_id);
  } 
}

static void broadcast_CmdMsg(int command_id, int target) {
  PRINTF("COMMAND: Broadcasting command %d\n", command_id);
  setCmdMsgId(&command, 31);
  command.cmd = command_id;
  command.target_id = target;
  linkaddr_t addr;
  addr.u8[0] = myAddress_1;
  addr.u8[1] = myAddress_2;
  encodeCmdMsg(&command, command_buffer);
  packetbuf_copyfrom(command_buffer, CMD_BUFFER_MAX_SIZE);
  int i;
  for (i = 0; i < NUMBER_OF_COWS; i++) {
    addr.u8[0] = register_cows[i];
    PRINTF("COMMAND Sending to %d.0\n", addr.u8[0]);
    mesh_send(&mesh, &addr);
  }
}

static int readRSSI() {
  // read RSSI function - to be removed from GW ?
  return packetbuf_attr(PACKETBUF_ATTR_RSSI) - 45;
}

/* Initialize mesh */
static void sent(struct mesh_conn *c) {
  //PRINTF("Packet sent\n");
}

static void timedout(struct mesh_conn *c) {
  sendFailedCounter += 1;
  PRINTF("MESSAGES: packet timedout. Failed to send packet counter: %d\n", sendFailedCounter);
}

static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops) {
  PRINTF("MESSAGES: Data received from %d.%d: %d bytes\n",from->u8[0], from->u8[1], packetbuf_datalen());
	

  // ACK
  if(packetbuf_datalen()==1){
    PRINTF("MESSAGES: Message ID: %d ACK received.\n", ((uint8_t *)packetbuf_dataptr())[0]);
    //ackMessage(&myPackets, ((uint8_t *)packetbuf_dataptr())[0]);
  }
  // Krava message
  else if ((((uint8_t *)packetbuf_dataptr())[0] & 0x03) == MSG_MESSAGE) {
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
  else if ((((uint8_t *)packetbuf_dataptr())[0] & 0x03) == MSG_CMD) { 
    CmdMsg command;
    decodeCmdMsg(packetbuf_dataptr(), &command);

    packetbuf_copyfrom(&command.id, 1);
    mesh_send(&mesh, from); // send ACK

    handleCommand(&command);
  } 
  // Emergency message
  else {
    PRINTF("EMERGENCY ");
    EmergencyMsg eMsg;
    decodeEmergencyMsg(packetbuf_dataptr(), &eMsg);

    packetbuf_copyfrom(&eMsg.id, 1);
    mesh_send(&mesh, from); // send ACK

    printEmergencyMsg(&eMsg);
  }
}

/* Address function */
static void setAddress(uint8_t myAddress_1, uint8_t myAddress_2) {  
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

  PRINTF("NETWORK: Routing table flush\n");
  route_flush_all();
}

static void handle_clusters() {
  PRINTF("CLUSTERS: Timer for cluster refresh expired\n");

  // Calculate scores for cluster head candidates
  int i;
  PRINTF("CLUSTERS: Cluster scores: \n");
  for (i = 0; i < NUMBER_OF_COWS; i++) {
    int score = 0;
    score += num_of_neighbours[i] * 1;
    score += motions[i] * -1;
    score += hops[i] * 2;
    score += batterys[i] / 100;

    cluster_scores[i] = score;
    PRINTF("%d-%d  ", register_cows[i], cluster_scores[i]);
  }
  PRINTF("\n");

  // TODO: find max nodes upto treshold

  // TODO: Create clusters 

  // TODO: Send cluster generation commands

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
  resetCmdMsg(&command);
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
    	PRINTF("SERIAL: received line: %s\n", (char *)data);
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
