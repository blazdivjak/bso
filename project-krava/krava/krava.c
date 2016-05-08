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
#include "../lib/libsensors.h"
#include "../lib/libmessage.h"


/*
* Krava
*/
PROCESS(krava, "krava");
AUTOSTART_PROCESSES(&krava);
PROCESS_THREAD(krava, ev, data)
{	
	//Our process
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	//static variables
	static struct etimer et;
	static int16_t decoded_temperature;
	tmp102_init();
	
	SENSORS_ACTIVATE(button_sensor);	
	while(1) {

		etimer_set(&et, CLOCK_SECOND*1);
		PROCESS_WAIT_EVENT();
		
		if(etimer_expired(&et)){
			decoded_temperature = decodeTemperature(tmp102_read_temp_raw());
			printTemperature(decoded_temperature);
			etimer_reset(&et);
		}
		
	}
	exit:
		leds_off(LEDS_ALL);
		PROCESS_END();
}