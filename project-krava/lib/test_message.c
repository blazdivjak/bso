#include <stdio.h>
#include "libmessage.h"

// to run this programm use: gcc -o test_message test_message.c libmessage.c && ./test_message
int main()
{
   
	Message m;
	m = resetMessage(m);

	printf("Temps: %d Accelerations: %d, Batterys: %d\n", m.tempsCount, m.accelerationsCount, m.batteriesCount);


	printf("Adding battery reading \n");
	m = addBattery(m, 10);
	m = addBattery(m, 16);
	m = addBattery(m, 65);
	printf("Temps: %d Accelerations: %d, Batterys: %d\n", m.tempsCount, m.accelerationsCount, m.batteriesCount);

	printf("Adding temperature reading \n");
	m = addTemperature(m, 11);
	m = addTemperature(m, -13);
	printf("Temps: %d Accelerations: %d, Batterys: %d\n", m.tempsCount, m.accelerationsCount, m.batteriesCount);

	printf("Adding acceleration reading \n");
	m = addAcceleration(m, 12);
	m = addAcceleration(m, 15);
	m = addAcceleration(m, 17);
	printf("Temps: %d Accelerations: %d, Batterys: %d\n", m.tempsCount, m.accelerationsCount, m.batteriesCount);

	printf("Calculating encoded data size \n");
	int size = getEncodeDataSize(m);
	printf("Encoded data size is: %d\n", size);

	printf("Encoding data to array of ints \n");
	int * enc = encodeData(m);
	int i;
	for(i = 0; i < size; i++) {
    	printf("%d ", enc[i]);
    }
    printf("\n");

    printf("Printing message \n");
    printMessage(m);

	printf("Encode message \n");
	char * encodedMessage = encode(m);
	printf("%s\n", encodedMessage);

	printf("Decoding encoded message\n");
	printMessage(decode(encodedMessage));

	printf("Reseting all readings \n");
	m = resetMessage(m);
	printf("Temps: %d Accelerations: %d, Batterys: %d\n", m.tempsCount, m.accelerationsCount, m.batteriesCount);

	return 0;
}