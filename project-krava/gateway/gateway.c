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
    //broadcast_CmdMsg(CMD_EMERGENCY_TWO, command->target_id);

  } else if (command->cmd == CMD_CANCEL_EMERGENCY_ONE) {
    printf("COMMAND: Emergency one cancel, cow id: %d\n", command->target_id);
    //broadcast_CmdMsg(CMD_CANCEL_EMERGENCY_ONE, command->target_id);

  } else if (command->cmd == CMD_CANCEL_EMERGENCY_TWO) {
    printf("COMMAND: Emergency two cancel, cow id: %d\n", command->target_id);
    //broadcast_CmdMsg(CMD_CANCEL_EMERGENCY_TWO, command->target_id);
  } 
}

static void broadcast_CmdMsg(uint8_t command_id, uint8_t target, uint8_t destinationAddress) {
  
  
  PRINTF("COMMAND: Broadcasting command %d\n", command_id);
  resetCmdMsg(&command);
  command.cmd = command_id;
  command.target_id = target;
  encodeCmdMsg(&command, command_buffer);
  packetbuf_copyfrom(command_buffer, CMD_BUFFER_MAX_SIZE);
  linkaddr_t sent_cmd_addr;
  sent_cmd_addr.u8[0] = destinationAddress;
  sent_cmd_addr.u8[1] = 0;
  PRINTF("COMMAND Sending to %d.0\n", sent_cmd_addr.u8[0]);
  mesh_send(&mesh, &sent_cmd_addr);
}

static int readRSSI() {
  // read RSSI function - to be removed from GW ?
  return packetbuf_attr(PACKETBUF_ATTR_RSSI) - 45;
}

/* Initialize mesh */
static void sent(struct mesh_conn *c) {
  PRINTF("NETWORK: Packet sent\n");
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

    // message decode
    Message m;
    decode(packetbuf_dataptr(), packetbuf_datalen(), &m);
    printMessage(&m);
    packetbuf_copyfrom(&m.id, 1);
    mesh_send(&mesh, from); // send ACK

    // find cows index in data structures
    int cow_index = find_cow_with_id(m.mote_id);

    // update hops count
    myhops[cow_index] = hops;

    // update info on how many times the cow is seen
    cows_seen_counter[cow_index] += 1;
    cows_seen_counter_status |= 1 << cow_index;
    
    // update status of sensors for each cow
    num_of_motions[cow_index] = m.motionCount;
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

    // find cows index in data structures
    int cow_index = find_cow_with_id(command.target);
    // update info on how many times the cow is seen
    cows_seen_counter[cow_index] += 1;
    cows_seen_counter_status |= 1 << cow_index;

    handleCommand(&command);
  } 
  // Emergency message
  else {
    PRINTF("EMERGENCY ");
    EmergencyMsg eMsg;
    decodeEmergencyMsg(packetbuf_dataptr(), &eMsg);

    packetbuf_copyfrom(&eMsg.id, 1);
    mesh_send(&mesh, from); // send ACK

    // find cows index in data structures
    int cow_index = find_cow_with_id(eMsg.mote_id);
    // update info on how many times the cow is seen
    cows_seen_counter[cow_index] += 1;
    cows_seen_counter_status |= 1 << cow_index;

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

static int get_average_motion(uint64_t motions, int motions_count) {
  // decodes motions and returns the average
  uint8_t motion_buffer[32];
  int sum = 0;
  int i;
  uint64_t tmp = motions;
  for (i = 0; i < motions_count; i++) {
    motion_buffer[i] = (uint8_t) (tmp & 0x03);
    tmp = tmp >> 2;
  }
  int num = sum / motions_count;
  return motion_buffer[0];
}

static void clusterAddMember(struct Cluster *c, uint8_t member) {
  c->members[c->members_count] = member;
  c->members_count++;
}

static int memberInCluster(struct Cluster *c, uint8_t member) {
  if (c->head == member) {
    return 1;
  }
  int i = 0;
  for (i = 0; i < c->members_count; i++) {
    if (c->members[i] == member) {
      return 1;
    }
  }
  return 0;
}

static void printCluster(struct Cluster *c) {
  PRINTF("CLUSTERS: Cluster %d constains %d members:", c->head, c->members_count);
  int i;
  for (i = 0; i < c->members_count; i++) {
    PRINTF("%d ", c->members[i]);
  }
  PRINTF("\n");
}

static void sendClusteringCommand(struct Cluster *c){
  // sends clustering command to cluster head and cluster members
  resetCmdMsg(&command);
  command.cmd = CMD_SET_LOCAL_GW;
  command.target_id = c->head;
  
  // send command to cluster members
  linkaddr_t addr;
  addr.u8[0] = myAddress_1;
  addr.u8[1] = myAddress_2;
  int i;
  for (i = 0; i < c->members_count; i++) {
    encodeCmdMsg(&command, command_buffer);
    packetbuf_copyfrom(command_buffer, CMD_BUFFER_MAX_SIZE);
    addr.u8[0] = c->members[i];
    PRINTF("CLUSTERS: Sending Command SET LOCAL GW to %d.0 where target is %d.0\n", addr.u8[0], command.target_id);
    mesh_send(&mesh, &addr);
    //clock_wait(192);
  }

  // send command to cluster head
  addr.u8[0] = c->head;
  PRINTF("CLUSTERS: Sending Command SET LOCAL GW to cluster head %d.0 where target is %d.0\n", addr.u8[0], command.target_id);
  encodeCmdMsg(&command, command_buffer);
  packetbuf_copyfrom(command_buffer, CMD_BUFFER_MAX_SIZE);
  mesh_send(&mesh, &addr);
}

static void handle_clusters() {
  printf("CLUSTERS: Timer for cluster refresh expired\n");
  // Calculate scores for cluster head candidates
  int i;
  printf("CLUSTERS: Cluster scores: ");
  for (i = 0; i < NUMBER_OF_COWS; i++) {
    int s_neighb = num_of_neighbours[i] * CLUSTER_FORMULA_WEIGHT_NUM_NEIGHBOURS;
    int s_motion = get_average_motion(motions[i], num_of_motions[i]) * -CLUSTER_FORMULA_WEIGHT_NUM_MOTION; 
    int s_myhops = myhops[i] * -CLUSTER_FORMULA_WEIGHT_NUM_HOPS;
    int s_batter = ((batterys[i] / 10) / 2) * CLUSTER_FORMULA_WEIGHT_NUM_BATTERY; // numers from 0 to 5 * WEIGHT
    // TODO: score += RSSI
    int score = 0 + s_neighb + s_motion + s_myhops + s_batter;
    //printf("CLUSTERS: i: %d N: %d + M: %d + H: %d + B: %d = %d\n", 
    //  i, s_neighb, s_motion, s_myhops, s_batter, score);
    cluster_scores[i] = score;
    printf("%d:%d  ", register_cows[i], cluster_scores[i]);
  }
  printf("\n");
  // find potencial clusters with score greather than treshold
  int cluster_candidates_count = 0;
  int cluster_candidates[NUMBER_OF_COWS];
  for (i = 0; i < NUMBER_OF_COWS; i++) {
    if (cluster_scores[i] > CLUSTER_FORMULA_TRESHOLD) {
      cluster_candidates[i] = register_cows[i];
      cluster_candidates_count += 1;
    } else {
      cluster_candidates[i] = -1;
    }
  }
  PRINTF("CLUSTERS: found %d potential clusters\n", cluster_candidates_count);
  if (cluster_candidates_count == 0) {
    return;
  }
  PRINTF("CLUSTERS: Potencial clusters are: ");
  for (i = 0; i < NUMBER_OF_COWS; i++){
    PRINTF("%d ", cluster_candidates[i]);
  }
  PRINTF("\n");
  // Choose cluster heads
  int cluster_candidates_count_2 = 0;
  struct Cluster cluster_candidates2[NUMBER_OF_COWS];
  for (i = 0; i < NUMBER_OF_COWS; i++) {
    if (cluster_candidates[i] == -1) {
      continue; // mote not candidate
    }
    int new_head_already_in_cluster = 0;
    int j = 0;
    for (j = 0; j < cluster_candidates_count_2; j++) { // check if new head already in any of clusters
      if (memberInCluster(&cluster_candidates2[j], cluster_candidates[i])) {
        new_head_already_in_cluster = 1;
        PRINTF("CLUSTERS: candidate %d found in %d\n", cluster_candidates[i], cluster_candidates2[j].head);
        break;
      }
    }
    if (!new_head_already_in_cluster) { // cluster can be added but not sure if all members also - filtering
      PRINTF("CLUSTERS: Adding mote %d to clusters!\n", cluster_candidates[i]);
      struct Cluster c;
      c.members_count = 0;
      c.head = cluster_candidates[i];
      int n;
      for (n = 0; n < num_of_neighbours[find_cow_with_id(cluster_candidates[i])]; n++) {
        new_head_already_in_cluster = 0;
        int j = 0;
        for (j = 0; j < cluster_candidates_count_2; j++) { // check if neighbour already in any of clusters
          if (memberInCluster(&cluster_candidates2[j], neighbours[find_cow_with_id(cluster_candidates[i])][n])) {
            new_head_already_in_cluster = 1;
          }
        }
        if (!new_head_already_in_cluster) {
          clusterAddMember(&c, neighbours[find_cow_with_id(cluster_candidates[i])][n]);
          PRINTF("CLUSTERS: adding member %d to cluster %d\n", neighbours[find_cow_with_id(cluster_candidates[i])][n], cluster_candidates[i]);
        }
      }
      cluster_candidates2[cluster_candidates_count_2] = c;
      cluster_candidates_count_2 += 1;
    }
  }
  PRINTF("CLUSTERS: %d clusters are forwarded to filtering\n", cluster_candidates_count_2);
  if (cluster_candidates_count_2 == 0) {
    return;
  }
  // filter out clusters with zero members
  clusters_count = 0;
  //struct Cluster final_clusters[NUMBER_OF_COWS];
  for (i = 0; i < cluster_candidates_count_2; i++) {
    printf("CLUSTERS: Cluster %d contains %d members\n", cluster_candidates2[i].head, cluster_candidates2[i].members_count);
    if (cluster_candidates2[i].members_count > 0) {
      clusters[clusters_count] = cluster_candidates2[i];
      clusters_count += 1;
    }
  }
  printf("CLUSTERS: %d clusters generated\n", clusters_count);
  if (clusters_count == 0) {
    return;
  }
  for (i=0; i < clusters_count; i++) {
    printCluster(&clusters[i]);
  }
  
  // Send cluster generation commands
  for (i = 0; i < clusters_count; i++) {
    sendClusteringCommand(&clusters[i]);
  }
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
    if (old_emergency_missing ^ status.emergency_missing) { // only send cancel if previous command was alarm
      PRINTF("COMMAND: Adding command %d taget: %d to buffer.\n", CMD_CANCEL_EMERGENCY_ONE, target);
      ringbuf_put(&commanderBuff, CMD_CANCEL_EMERGENCY_ONE);
      ringbuf_put(&commanderBuff, target);
    }
  }

  // disable alarm if its on for more than cows seen alarm window
  if (status.emergency_missing > 0) {
    cows_seen_alarm_window -= 1;
    if (cows_seen_alarm_window <= 0) { // disable alarm after cows_seen_alarm_window iterations of alarm
      status.emergency_missing = 0;
      cows_seen_alarm_window = COWS_SEEN_ALARM_WINDOW;    
      PRINTF("COMMAND: Adding command %d taget: %d to buffer.\n", CMD_CANCEL_EMERGENCY_ONE, target);        
      ringbuf_put(&commanderBuff, CMD_CANCEL_EMERGENCY_ONE);
      ringbuf_put(&commanderBuff, target);      
    }
  }

  if (status.emergency_missing > 0) {
    //TODO: Add command to buffer
    PRINTF("COMMAND: Adding command %d taget: %d to buffer.\n", CMD_CANCEL_EMERGENCY_ONE, target);    
    ringbuf_put(&commanderBuff, CMD_EMERGENCY_ONE);
    ringbuf_put(&commanderBuff, target);      
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
PROCESS(commander, "Commander proces");
AUTOSTART_PROCESSES(&gateway_main, &commander);

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
      if (!memcmp(CMD_NUMBER_OF_MOTES, data, 3)) {
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
      } else if (!memcmp(CMD_QUERY, data, 3)) {
          uint8_t qId = atoi(&data[3]);
          PRINTF("QUERY COMMAND: Mote ID = %d\n", qId);
          CmdMsg cmd;
          resetCmdMsg(&cmd);
          cmd.cmd = CMD_QUERY_MOTE;
          cmd.target_id = qId;
          encodeCmdMsg(&cmd, command_buffer);
          linkaddr_t addr;
          addr.u8[0] = qId;
          addr.u8[1] = myAddress_2;
          packetbuf_copyfrom(command_buffer, CMD_BUFFER_MAX_SIZE);
          PRINTF("COMMAND Sending to %d.0\n", addr.u8[0]);
          mesh_send(&mesh, &addr);
      }
    }

  }
  PROCESS_END();
}

PROCESS_THREAD(commander, ev, data)
{ 
  //Our process 
  PROCESS_BEGIN();
  PRINTF("Command process\n");

  ringbuf_init(&commanderBuff, commanderBuff_data, sizeof(commanderBuff_data));    
  etimer_set(&commander_interval, COMMAND_SEND_INTERVAL*100);
  
  static volatile uint8_t currentCommand;
  static volatile uint8_t currentTarget;  
  static volatile int i;

  PROCESS_WAIT_EVENT();
  ringbuf_init(&commanderBuff, commanderBuff_data, sizeof(commanderBuff_data));
  etimer_set(&commander_interval, COMMAND_SEND_INTERVAL);

  //Process main loop
  while(1) {
        
    PROCESS_WAIT_EVENT();
    
    if(etimer_expired(&commander_interval)){
      PRINTF("COMMAND: Checking for pending commands. Number: %d\n", ringbuf_elements(&commanderBuff));
      if(ringbuf_elements(&commanderBuff)>1){    
        currentCommand=ringbuf_get(&commanderBuff);
        currentTarget=ringbuf_get(&commanderBuff);
        PRINTF("COMMAND: Initializing sending command: %d target: %d\n", currentCommand, currentTarget);
        etimer_set(&commander_interval, (CLOCK_SECOND)/5);
        
        for(i=0;i<NUMBER_OF_COWS;i++){
          PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&commander_interval));          
          broadcast_CmdMsg(currentCommand, currentTarget, register_cows[i]);
          etimer_reset(&commander_interval);
        }
        etimer_set(&commander_interval, COMMAND_SEND_INTERVAL);      
      }else{
        etimer_reset(&commander_interval);
      }
    }                     
  }
  PROCESS_END();
}