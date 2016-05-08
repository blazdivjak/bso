/*
*__author: blaz__
*__date: 2016-03-07__
*__assigment1__
*/
#include <stdio.h>
#include <stdlib.h>
#include "contiki.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/i2cmaster.h"  // Include IC driver
#include "dev/tmp102.h"     // Include sensor driver
#include "dev/battery-sensor.h"
#include "dev/adxl345.h"
#include "../lib/libsensors.h"
#include "../lib/libmessage.h"

#define MOVEMENT_READ_INTERVAL (CLOCK_SECOND)*1
#define RSSI_READ_INTERVAL (CLOCK_SECOND)*5
#define TEMP_READ_INTERVAL (CLOCK_SECOND)*30
#define BATTERY_READ_INTERVAL (CLOCK_SECOND)*30

/*---------------------------------------------------------------------------*/
/* Helper functions															 */
/*---------------------------------------------------------------------------*/

void readBattery(){
  
    uint16_t bateria = battery_sensor.value(0);
    float mv = (bateria * 2.500 * 2) / 4096;
    printf("Battery: %i (%ld.%03d mV)\n", bateria, (long)mv,
           (unsigned)((mv - myFloor(mv)) * 1000));
}  
void readTemperature(){
	
	static int16_t decoded_temperature;
	decoded_temperature =  decodeTemperature(tmp102_read_temp_raw());	
	printTemperature(decoded_temperature);
}
void readMovement(){
	
	int16_t x, y, z;	

	x = adxl345.value(X_AXIS);
    y = adxl345.value(Y_AXIS);
    z = adxl345.value(Z_AXIS);
        
    printf("Movement: x: %d y: %d z: %d\n",x, y, z);    
}
void readRSSI(){

	printf("Reading RSSI...\n");
}


/*
*Process definitions
*/
PROCESS(krava, "Krava");
AUTOSTART_PROCESSES(&krava);

/*
* Krava process
*/
PROCESS_THREAD(krava, ev, data)
{	
	//Our process
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	//Initialize timers for intervals
	static struct etimer movementReadInterval;
	static struct etimer temperatureReadInterval;	
	static struct etimer rssiReadInterval;

	etimer_set(&movementReadInterval, MOVEMENT_READ_INTERVAL);
	etimer_set(&rssiReadInterval, RSSI_READ_INTERVAL);
	etimer_set(&temperatureReadInterval, TEMP_READ_INTERVAL);	
	
	//Initialize sensors
	tmp102_init();
	
	SENSORS_ACTIVATE(button_sensor);	
	SENSORS_ACTIVATE(battery_sensor);
	SENSORS_ACTIVATE(adxl345);

	//Process main loop
	while(1) {

		PROCESS_WAIT_EVENT();
		
		if(etimer_expired(&movementReadInterval)){
			readMovement();
			etimer_reset(&movementReadInterval);
		}
		if(etimer_expired(&rssiReadInterval)){
			readRSSI();
			etimer_reset(&rssiReadInterval);
		}
		if(etimer_expired(&temperatureReadInterval)){
			readTemperature();
			readBattery();			
			etimer_reset(&temperatureReadInterval);
		}		
	}
	exit:		
		SENSORS_DEACTIVATE(button_sensor);
		SENSORS_DEACTIVATE(battery_sensor);	
		SENSORS_DEACTIVATE(adxl345);	
		leds_off(LEDS_ALL);
		PROCESS_END();
}
