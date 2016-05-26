#include <stdio.h>
#include <stdint.h>
#include "libmessage.h"
#include <inttypes.h>

// to run this programm use: gcc -o test_message test_message.c libmessage.c && ./test_message
int main()
{
    int i;
	Message m;
	resetMessage(&m);

	printf("Motions: %d, neighbours: %d\n", m.motionCount, m.neighbourCount);


	printf("Adding motions reading \n");
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

	// printf("Reseting all readings \n");
	// m = resetMessage(m);
	// printf("Temps: %d Accelerations: %d, Batterys: %d\n", m.tempsCount, m.accelerationsCount, m.batteriesCount);

	return 0;
}