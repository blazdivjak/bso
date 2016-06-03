#include <stdio.h>
#include <stdint.h>
#include "libmessage.h"
#include <inttypes.h>
#include <stdlib.h>


// to run this programm use: 
// gcc -o test_message test_message.c libmessage.c && ./test_message
int main()
{
    int i;
	Message m;
	resetMessage(&m);

	printf("Motions: %d, neighbours: %d\n", m.motionCount, m.neighbourCount);


	printf("Adding motions reading \n");
	m.mote_id = 5;
	m.temp = 21;
	addMotion(&m, 0);
	addMotion(&m, 1);
	addMotion(&m, 2);
	addMotion(&m, 3);
	addMotion(&m, 2);
	addMotion(&m, 1);
	printf("Motions: 0x%0x\n", (uint16_t) (m.motions & 0xFFFFFFFF));
	static uint8_t motions[10];
	getMotionArray(&m, motions);
	for (i = 0; i < m.motionCount; i++) {
		printf("%02x ", motions[i]);
	}
	printf("\nMotions: %d, neighbours: %d\n", m.motionCount, m.neighbourCount);

	printf("Adding neighbours \n");
	addNeighbour(&m, 11);
	addNeighbour(&m, 11);
	addNeighbour(&m, 113);
	addNeighbour(&m, 113);
	addNeighbour(&m, 113);
	addNeighbour(&m, 113);
	addNeighbour(&m, 113);
	printf("Motions: %d, neighbours: %d\n", m.motionCount, m.neighbourCount);

	printMessage(&m);

	printf("Encoding data to array of ints \n");
	uint8_t buffer[35];
	uint8_t size = encodeData(&m, buffer);
	for (i = 0; i < size; i++) {
		printf("%02x ", buffer[i]);
	}
	printf("\nEncoded data size is: %d, calculated is: %d\n", size, getEncodeDataSize(&m));

    printf("Printing message \n");
    printMessage(&m);

	printf("Decoding encoded message\n");
	Message mNew;
	decode(buffer, size, &mNew);
	printMessage(&mNew);

	printf("Copy Message\n");
	resetMessage(&mNew);
	printMessage(&mNew);
	mNew = m;
	printMessage(&mNew);

	printf("\n\nPackets...\n");
	Packets p;
	resetPackets(&p);
	addMessage(&p, &m);
	mNew.id++;
	addMessage(&p, &mNew);
	printf("Message ids in packets: ");
	for (i=0; i<p.count; i++) {
		printf("%d, ", p.payload[i].id);
	}
	printf("\n");
	ackMessage(&p, m.id);
	printf("Message ids in packets: ");
	for (i=0; i<p.count; i++) {
		printf("%d, ", p.payload[i].id);
	}
	printf("\n");


	printf("\n\nGateway Message\n");
	CmdMsg g;
	setCmdMsgId(&g, 32);
	g.cmd = CMD_EMERGENCY_ONE;
	g.target_id = 6;
	encodeCmdMsg(&g, buffer);
	printf("buffer[0]=%d\n", buffer[0]);
	CmdMsg g2;
	decodeCmdMsg(buffer, &g2);
	printCmdMsg(&g2);

	printf("\n\nEmergency Message\n");
	EmergencyMsg e, eNew;
	setEmergencyMsgType(&e, MSG_E_TWO_RSSI);
	setEmergencyMsgId(&e, 7);

	addEmergencyData(&e, 0);
	addEmergencyData(&e, 1);
	addEmergencyData(&e, 2);
	addEmergencyData(&e, 3);
	addEmergencyData(&e, 4);

	encodeEmergencyMsg(&e, buffer);
	decodeEmergencyMsg(buffer, &eNew);

	printEmergencyMsg(&e);
	printEmergencyMsg(&eNew);


	return 0;
}