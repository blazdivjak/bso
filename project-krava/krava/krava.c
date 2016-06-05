/*
*__authors: blaz, gregor, gasper__
*__date: 2016-04-15__
*__project krava__
*/
#include "krava.h"

#define DEBUG 1

#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*
* Mesh functions
*/

static void sent(struct mesh_conn *c)
{
  PRINTF("NETWORK: packet sent\n");
}
static void timedout(struct mesh_conn *c)
{
  //TODO: Move this function to ACK receive function
  sendFailedCounter += 1;
  PRINTF("NETWORK: packet timedout. Failed to send packet counter: %d\n", sendFailedCounter);
}

static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops) {
   
  PRINTF("MESSAGES: Data received from %d.%d: %d bytes\n",from->u8[0], from->u8[1], packetbuf_datalen());
  // PRINTF("buffer[0]=0x%x\n", (((uint8_t *)packetbuf_dataptr())[0] & 0x03));
  //ACK
  if(packetbuf_datalen()==1){
  	PRINTF("MESSAGES: Message ID: %d ACK received.\n", ((uint8_t *)packetbuf_dataptr())[0]);
  	ackMessage(&myPackets, ((uint8_t *)packetbuf_dataptr())[0]);
  	status.ackCounter+=1;
  }
  //Krava message
  else if((((uint8_t *)packetbuf_dataptr())[0] & 0x03) == MSG_MESSAGE){
  	//Send ACK for packet  	
  	packetbuf_copyfrom(packetbuf_dataptr(), 1);
    mesh_send(&mesh, from); // send ACK

    #ifdef DEBUG
    Message fMsg;
  	decode(((uint8_t *)packetbuf_dataptr()), packetbuf_datalen(), &fMsg);
  	printMessage(&fMsg);
  	#endif
  	
  	//Forward message
	forward(((uint8_t *)packetbuf_dataptr()), packetbuf_datalen());

  }
  //Gateway command
  else if((((uint8_t *)packetbuf_dataptr())[0] & 0x03) == MSG_CMD){
  	CmdMsg command;
    decodeCmdMsg(packetbuf_dataptr(), &command);

    packetbuf_copyfrom(&command.id, 1);
    mesh_send(&mesh, from); // send ACK

    handleCommand(&command);
  }   
  /*
  IF I am local gateway I can also receive RSSI or ACCELEROMETER mesurements from cows in my cluster  and
  I need to forward them to default gateway
  */
  else {
  	//Send ACK for packet  	
  	packetbuf_copyfrom(packetbuf_dataptr(), 1);
    mesh_send(&mesh, from); // send ACK

    #ifdef DEBUG
    EmergencyMsg eMsg;
	decodeEmergencyMsg(packetbuf_dataptr(), &eMsg);
  	printEmergencyMsg(&eMsg);
  	#endif

  	//Forward emergency
	forward(((uint8_t *)packetbuf_dataptr()), packetbuf_datalen());
  } 

}

/*
Handle gateway commands
*/

void handleCommand(CmdMsg *command) {
  
  PRINTF("COMMAND: Processing command\n"); 	

  if(command->cmd == CMD_SET_LOCAL_GW) {
    PRINTF("COMMAND: Set local gateway: %d\n", command->target_id);

    //set gateway
    if(command->target_id!=node_id){

    	//Configure as gateway
    	PRINTF("COMMAND: Setting new Gateway.\n");
    	status.iAmGateway = 0;
    	status.iAmInCluster = 1;
    	currentGateway = command->target_id;
    	
    	//Change power based on RSSI and also adjust RSSI for neighbor detection
    	setPower(15);
    }else{
    	PRINTF("COMMAND: I am new local gateway.\n");
    	status.iAmInCluster = 0;
    	status.iAmGateway = 1;
    	setPower(CC2420_TXPOWER_MAX);
    	currentGateway = defaultGateway;
    }		    
	etimer_set(&clusterRefreshInterval, CLUSTER_INTERVAL);	

  } else if (command->cmd == CMD_QUERY_MOTE) {
    PRINTF("COMMAND: Query from gateway: %d\n", command->target_id);
    sendMessage();
  } else if (command->cmd == CMD_EMERGENCY_ONE) {
    PRINTF("COMMAND: Emergency one, cow unreachable id: %d\n", command->target_id);
    handleEmergencyOne();
  } else if (command->cmd == CMD_EMERGENCY_TWO) {
    PRINTF("COMMAND: Emergency two, cow running id: %d\n", command->target_id);
    handleEmergencyTwo(command->target_id);
  } else if (command->cmd == CMD_CANCEL_EMERGENCY_ONE) {
    PRINTF("COMMAND: Emergency one cancel, cow id: %d\n", command->target_id);
    cancelSysEmergencyOne();
  } else if (command->cmd == CMD_CANCEL_EMERGENCY_TWO) {
    PRINTF("COMMAND: Emergency two cancel, cow id: %d\n", command->target_id);
    cancelSysEmergencyTwo();
  }
}

/*
Cluster handling
*/

void clusterLearningMode(){

	//Disable cluster
	PRINTF("CLUSTERS: Initializing cluster learning mode.\n");
	status.iAmGateway = 0;
	status.iAmInCluster = 0;

	//Initialize neighbor discovery
	PRINTF("CLUSTERS: Searching for neighbors.\n");

	//Full powah owjea
	setPower(CC2420_TXPOWER_MAX);

	//Set to default gateway
	PRINTF("CLUSTERS: Setting my current gateway to: %d.0.\n", defaultGateway);
	currentGateway = defaultGateway;

	//Flushing neighbors
	PRINTF("CLUSTERS: Flushing neighbor table\n");
	m.neighbourCount = 0;

	//etimer_set(&clusterRefreshInterval, CLUSTER_INTERVAL);

}

/*
Emergency mode handling
*/

void handleEmergencyOne() {

	//Reconfigure timers
	PRINTF("EMERGENCY: #2 Searching for lost krava.\n");

	status.emergencyOne = 2;

	//full power
	setPower(CC2420_TXPOWER_MAX);
}

void handleEmergencyTwo(uint8_t target) {

	//If I am the running krava dont bother to monitor :)
	if(status.emergencyTwo==1 || m.mote_id == target) {
		return;
	}
	//Monitor rssi for this krava
	else {
		PRINTF("EMERGENCY: Starting running krava monitoring.\n");

		//Configure broadcast listening timer and sense more offten
		neighbor_sense_interval = NEIGHBOR_SENSE_INTERVAL/2;		
		status.emergencyTwo = 2;
		status.emergencyTarget = target;
	}	
}

void cancelSysEmergencyOne() {

	neighbor_sense_interval =  NEIGHBOR_SENSE_TIME;
	mesh_refresh_interval = MESH_REFRESH_INTERVAL;
	
	if(status.emergencyOne != 0) {
		
		//Back to previous power level		
		if(status.iAmInCluster){
			setPower(15);	
		}else{
			setPower(CC2420_TXPOWER_MAX);
		}		

		status.emergencyOne = 0;
	}

}

void cancelSysEmergencyTwo() {

	neighbor_sense_interval =  NEIGHBOR_SENSE_INTERVAL;
	mesh_refresh_interval = MESH_REFRESH_INTERVAL;

	PRINTF("EMERGENCY cancel sys #2\n");

	if(status.emergencyTwo==2){
		//TODO: Send data
		sendEmergencyTwoRSSI();
		status.emergencyTwo = 0;
	} 
	else if(status.emergencyTwo==1) {
		sendEmergencyTwoAcc();
		status.emergencyTwo = 0;
	}
}

void toggleEmergencyOne() {
	
	if(status.ackCounter==0) {
		PRINTF("EMERGENCY: #1 Lost connectivity to gateway.\n");
		status.emergencyOne = 1;
		currentGateway = DEFAULT_GATEWAY_ADDRESS;
		mesh_refresh_interval = MESH_REFRESH_INTERVAL/10;
		etimer_set(&meshRefreshInterval, mesh_refresh_interval);
		
		//Full power & mesh reinitialize
		setPower(CC2420_TXPOWER_MAX);

	} else {
		status.ackCounter=0;	
		status.emergencyOne=0;
		mesh_refresh_interval = MESH_REFRESH_INTERVAL;	
		etimer_set(&meshRefreshInterval, mesh_refresh_interval);
	}	
}

static void triggerEmergencyTwo() {
	
	if (status.emergencyTwo == 1) {		
		return;
	}
	
	status.emergencyTwo = 1;
	PRINTF("EMERGENCY: Emergency Two triggered\n");

	resetCmdMsg(&command);
	command.cmd = CMD_EMERGENCY_TWO;
	command.target_id = m.mote_id;
	sendCommand();	

	//Advertise offten and mesure movement more offten	
	neighbor_advertisment_interval = NEIGHBOR_ADVERTISEMENT_INTERVAL/2;
	movement_read_interval = MOVEMENT_READ_INTERVAL/2;

}

static void cancelEmergencyTwo() {
	
	running_counter = 0;
	
	if (status.emergencyTwo == 0) {
		return;
	} 

	PRINTF("EMERGENCY: Emergency Two canceled\n");
	status.emergencyTwo = 0;
	resetCmdMsg(&command);
	command.cmd = CMD_CANCEL_EMERGENCY_TWO;
	command.target_id = m.mote_id;
	sendCommand();
	sendEmergencyTwoAcc();
	
	neighbor_advertisment_interval = NEIGHBOR_ADVERTISEMENT_INTERVAL;
	movement_read_interval = MOVEMENT_READ_INTERVAL;
}

/*
Power handling
*/

void setPower(uint8_t powerLevel) {
		
	txpower = cc2420_get_txpower();
	PRINTF("POWER: Previous: %d Now: %d\n", txpower, powerLevel);
	cc2420_set_txpower(powerLevel);	

}

/*
* Communication functions
*/
static void setAddress(uint8_t myAddress_1, uint8_t myAddress_2) {  
  m.mote_id = myAddress_1;
  eTwoRSSI.mote_id = myAddress_1;
  eTwoAcc.mote_id = myAddress_1;

  linkaddr_t addr;
  addr.u8[0] = myAddress_1;
  addr.u8[1] = myAddress_2;  
  PRINTF("NETWORK: My Address: %d.%d\n", addr.u8[0],addr.u8[1]);
  uint16_t shortaddr = (addr.u8[0] << 8) + addr.u8[1];
  cc2420_set_pan_addr(IEEE802154_PANID, shortaddr, NULL);   
  linkaddr_set_node_addr (&addr);	
}

static void setCurrentGateway(uint8_t currentGatewayAddress) {

	PRINTF("NETWORK: Setting current gateway to: %d\n", currentGatewayAddress);
	currentGateway = currentGatewayAddress;
}

void sendCommand() {
	
	PRINTF("COMMAND: Sending command to my current gateway ID: %d.0, %d bytes\n", currentGateway, CMD_BUFFER_MAX_SIZE);

	linkaddr_t addr_send;     
	encodeCmdMsg(&command, command_buffer);  
	packetbuf_copyfrom(command_buffer, CMD_BUFFER_MAX_SIZE);       
	addr_send.u8[0] = currentGateway;
	addr_send.u8[1] = 0;  
	mesh_send(&mesh, &addr_send);
	resetCmdMsg(&command);
}

void forward(uint8_t *buffer, uint8_t length) {
	PRINTF("MESSAGES: Forwarding message type=%u id=%u to current gateway: %d.0\n", 
		(buffer[0]&0x3), (buffer[0]>>2), currentGateway);
	linkaddr_t addr_send;
	packetbuf_copyfrom(buffer, length);
	addr_send.u8[0] = currentGateway;
	addr_send.u8[1] = 0;
	mesh_send(&mesh, &addr_send);
}

void sendMessage() {

	setMsgId(&m, m.id+1);

	//Copy message  
	addMessage(&myPackets, &m);

	//TODO: Poslji komplet pakete

	linkaddr_t addr_send; 
	uint8_t size = encodeData(&m, send_buffer);
	PRINTF("MESSAGES: Sending message to my current gateway ID: %d.0, %d bytes\n", currentGateway, size);
	packetbuf_copyfrom(send_buffer, size);       
	addr_send.u8[0] = currentGateway;
	addr_send.u8[1] = 0;
	mesh_send(&mesh, &addr_send);
	resetMessage(&m);
}

void sendEmergencyTwoRSSI() {
   
	linkaddr_t addr_send;     
	uint8_t size = encodeEmergencyMsg(&eTwoRSSI, emergencyBuffer);  
	PRINTF("EMERGENCY #2. Sending RSSI to my current gateway ID: %d.0, %d bytes\n", currentGateway, size);
	packetbuf_copyfrom(emergencyBuffer, size);       
	addr_send.u8[0] = currentGateway;
	addr_send.u8[1] = 0;  
	mesh_send(&mesh, &addr_send);
	resetEmergencyMsg(&eTwoRSSI);
}

void sendEmergencyTwoAcc() {
    
	linkaddr_t addr_send;     
	uint8_t size = encodeEmergencyMsg(&eTwoAcc, emergencyBuffer);  
	PRINTF("EMERGENCY #2. Sending Accelerations to my current gateway ID: %d.0, %d bytes\n", currentGateway, size);
	packetbuf_copyfrom(emergencyBuffer, size);
	addr_send.u8[0] = currentGateway;
	addr_send.u8[1] = 0;  
	mesh_send(&mesh, &addr_send);
	resetEmergencyMsg(&eTwoAcc);
}


/*
* Initialize broadcast connection
*/

static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{	

  int rssi;	
  rssi = readRSSI();
  PRINTF("Neighbor advertisment received from %d.%d: '%s' RSSI: %d, eTarget: %d, emergencyTwo: %d\n", from->u8[0], from->u8[1], (char *)packetbuf_dataptr(), rssi, status.emergencyTarget, status.emergencyTwo);
      
  //If emergency
  if((status.emergencyTwo == 2) && (status.emergencyTarget == from->u8[0])){
  	PRINTF("EMERGENCY Adding RSSI to eTwoRSSI\n");
  	//Save to RSSI mesurements for lost krava
	if (addEmergencyData(&eTwoRSSI, (uint8_t) (-1*readRSSI())) == EMERGENCY_DATA_MAX) {
		sendEmergencyTwoRSSI();
	}

  }  

  //Check message rssi for adding only close neighbors  
  if (rssi>=rssiTreshold){
  	addNeighbour(&m, from->u8[0]);
	PRINTF("NETWORK: Neighbor detected adding %d to table. RSSI: %d, Current number of neighbors: %d.\n",from->u8[0],rssi, m.neighbourCount);
  }    
}

/*
* Sensor functions															 
*/
void readBattery(){
  
    //uint16_t bateria = battery_sensor.value(0);
    //float mv = (bateria * 2.500 * 2) / 4096;
    //PRINTF("Battery: %i (%ld.%03d mV)\n", bateria, (long)mv,(unsigned)((mv - myFloor(mv)) * 1000));
    m.battery = decodeBattery(battery_sensor.value(0));

}  
void readTemperature(){
	
	static int8_t decoded_temperature;
	decoded_temperature =  decodeTemperature(tmp102_read_temp_raw());	
	//printTemperature(decoded_temperature);
	m.temp = decoded_temperature;
}

void readMovement(){
	
	int64_t x, y, z;	

	x = adxl345.value(X_AXIS);
    y = adxl345.value(Y_AXIS);
    z = adxl345.value(Z_AXIS);       
    //PRINTF("Movement: x: %d y: %d z: %d\n",x, y, z);  
    int64_t acc =  x*x + y*y + z*z;
    average_movement = average_movement + (acc - average_movement)/IIR_STRENGTH;
    if (movement_counter%MOVEMENT_COUNTER_VAL == 0) {
	    if (average_movement < WALKING_TRESHOLD) {
	    	addMotion(&m, STANDING);
	    	//PRINTF("Standing \t");
	    	cancelEmergencyTwo();
	    } else if (average_movement < RUNNING_TRESHOLD) {
	    	addMotion(&m, WALKING);
	    	//PRINTF("Walking \t");
	    	cancelEmergencyTwo();
	    } else {
	    	addMotion(&m, RUNNING);
	    	//PRINTF("Running \t");
	    	if (running_counter != 0xFF) {
	    		running_counter++;
	    	}
	    }

	    if (running_counter >= RUNNING_MAX) {
	    	triggerEmergencyTwo();
	    }

	    //PRINTF("Acce: %" PRId64 "\tAvg: %" PRId64 "\n", acc, average_movement);
	}
	movement_counter++;
	if (status.emergencyTwo == 1) {
		if (addEmergencyData(&eTwoAcc, (uint8_t) (acc/10000)) == EMERGENCY_DATA_MAX) {
			PRINTF("EMERGENCY #2 Accelerations full\n");
			sendEmergencyTwoAcc();
		}
	}

    
    //TODO: Compare with previous motion and find 0,1,2,3 motion statuses.

}
int readRSSI(){
		
  return packetbuf_attr(PACKETBUF_ATTR_RSSI) - 45;
}

/*
*Process definitions
*/
PROCESS(krava, "Krava");
PROCESS(communication, "Communication");
PROCESS(neighbors, "Sense neigbors");
AUTOSTART_PROCESSES(&krava, &communication, &neighbors);
/*
* Krava process
*/
PROCESS_THREAD(krava, ev, data)
{	
	//Our process
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	PRINTF("Sensor sensing process\n");
	status.iAmGateway = 0;
	status.emergencyOne = 0;
	status.emergencyTwo = 0;	 
	resetMessage(&m);
	resetMessage(&mNew);
	resetPackets(&myPackets);
	resetPackets(&otherKravaPackets);
	resetCmdMsg(&command);
	resetEmergencyMsg(&eTwoRSSI);
	setEmergencyMsgType(&eTwoRSSI, MSG_E_TWO_RSSI);
	resetEmergencyMsg(&eTwoAcc);
	setEmergencyMsgType(&eTwoAcc, MSG_E_TWO_ACC);

	//Initialize timers for intervals
	etimer_set(&movementReadInterval, movement_read_interval);
	etimer_set(&rssiReadInterval, rssi_read_interval);
	etimer_set(&temperatureReadInterval, temp_read_intreval);	
	
	//tmp102_init();
	SENSORS_ACTIVATE(tmp102);
	SENSORS_ACTIVATE(battery_sensor);
	SENSORS_ACTIVATE(adxl345);
	SENSORS_ACTIVATE(tmp102);				
		
	//Process main loop
	while(1) {

		PROCESS_WAIT_EVENT();
		
		if(etimer_expired(&movementReadInterval)) {		
			readMovement();
			etimer_set(&movementReadInterval, movement_read_interval);			
		}		
		if(etimer_expired(&temperatureReadInterval)) {
			readTemperature();
			readBattery();			
			etimer_set(&temperatureReadInterval, temp_read_intreval);					
		}		
	}
	exit:				
		SENSORS_DEACTIVATE(battery_sensor);
		SENSORS_DEACTIVATE(adxl345);
		SENSORS_DEACTIVATE(tmp102);
		leds_off(LEDS_ALL);
		PROCESS_END();
}

/*
* Communication process
*/
PROCESS_THREAD(communication, ev, data)
{	
	//Our process		
	PROCESS_BEGIN();

	resetMessage(&m);
	PRINTF("Communication process\n");	
	setAddress(node_id, myAddress_2);
	setCurrentGateway(defaultGateway);	
		
	etimer_set(&sendInterval, send_interval);
	etimer_set(&meshRefreshInterval, mesh_refresh_interval);
	etimer_set(&ackCountInterval, ack_count_interval);
	mesh_open(&mesh, 14, &callbacks);

	//Initialize sensors
	SENSORS_ACTIVATE(button_sensor);

	//Process main loop
	while(1) {
	 
		PROCESS_WAIT_EVENT();
		//set address to address in settings
		if((ev==sensors_event) && (data == &button_sensor)){	
      		setAddress(myAddress_1, myAddress_2);
    	}		
    	//sent message
		if(etimer_expired(&sendInterval)){
			sendMessage();
			etimer_set(&sendInterval, send_interval);			
		}
		//check if we are lost :)
		if(etimer_expired(&ackCountInterval)){
			toggleEmergencyOne();
			etimer_reset(&ackCountInterval);
		}		
		if(etimer_expired(&meshRefreshInterval)){
			PRINTF("NETWORK: Routing table flush\n");						
			route_flush_all();			
			etimer_set(&meshRefreshInterval, mesh_refresh_interval);
		}
	}	
	SENSORS_DEACTIVATE(button_sensor);
	PROCESS_END();
}

/*
Sense neighbors around and sent packet to broadcast so others can sense you krava :)
*/
PROCESS_THREAD(neighbors, ev, data)
{	
	//Our process	
	PROCESS_BEGIN();
	PRINTF("Neighbor discovery process\n");

	etimer_set(&neighborAdvertismentInterval, neighbor_advertisment_interval*60*5);
	etimer_set(&neighborSenseInterval, neighbor_sense_interval*60*5);
	etimer_set(&neighborSenseTime, neighbor_sense_time);	
	uint8_t sensing = 0;	
	//Process main loop

	PROCESS_WAIT_EVENT();
	etimer_set(&neighborAdvertismentInterval, neighbor_advertisment_interval);
	etimer_set(&neighborSenseInterval, neighbor_sense_interval);
	//etimer_set(&clusterRefreshInterval, CLUSTER_INTERVAL);

	while(1) {
		
		PROCESS_WAIT_EVENT();

		//Cluster reinitialization interval
		if(etimer_expired(&clusterRefreshInterval)){

			//If I am local gateway or a member of cluster disable cluster setting and procceed with 1 minute learning mode for new cluster setup
			if(status.iAmGateway == 1 || status.iAmInCluster == 1){
				clusterLearningMode();
			}
		}

		//sense neighbors every cca 5s for 1s
		if(etimer_expired(&neighborSenseInterval)){
			
			if(sensing==0){
				broadcast_open(&broadcast, 129, &broadcast_call);
				//PRINTF("Sensing for neighbors\n");								
				etimer_set(&neighborSenseInterval, neighbor_sense_time);
				sensing = 1;
			}	
			else{
				broadcast_close(&broadcast);
				//PRINTF("Sensing stoped\n");					
				etimer_set(&neighborSenseInterval, neighbor_sense_interval + random_rand() % (neighbor_sense_interval));
				sensing = 0;
			}														
		}
								
		//send advertisment to your neighbors every 5s
		if(etimer_expired(&neighborAdvertismentInterval)){
			
			PRINTF("Neighbor advertisment.\n");

			broadcast_open(&broadcast, 129, &broadcast_call);
 			packetbuf_copyfrom("Hello", 5);    
    		broadcast_send(&broadcast);
    		//PRINTF("Neighbor advertisment sent\n");
			etimer_set(&neighborAdvertismentInterval, neighbor_advertisment_interval + random_rand() % (neighbor_advertisment_interval));
			broadcast_close(&broadcast);
		}				
	}
	PROCESS_END();
}
