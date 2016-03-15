/*
*__author: blaz__
*__date: 2016-03-14__
*__assigment2__
*/
#include "contiki.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include <stdio.h>
#include <stdlib.h>
#include "net/rime/rime.h"
#include "net/rime/mesh.h"
#include "dev/i2cmaster.h"  // Include IC driver
#include "dev/tmp102.h"     // Include sensor driver
#define TMP102_READ_INTERVAL (CLOCK_SECOND)  // Poll the sensor every 500 ms

/*
* Our assignment2 process
*/
#define MESSAGE "Hello"

/*Static values*/

static struct mesh_conn mesh;
/*---------------------------------------------------------------------------*/
PROCESS(assignment2, "Assignment2 proces");
PROCESS (temp_process, "Test Temperature process");
AUTOSTART_PROCESSES(&assignment2,&temp_process);
/*---------------------------------------------------------------------------*/
static void sent(struct mesh_conn *c)
{
  printf("packet sent\n");

  //remove buffered value

}

static void timedout(struct mesh_conn *c)
{
  printf("packet timedout\n");

  //add value to buffer and try to send it next time

}

static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops)
{
  printf("Data received from %d.%d: %.*s (%d)\n",
	 from->u8[0], from->u8[1],
	 packetbuf_datalen(), (char *)packetbuf_dataptr(), packetbuf_datalen());

  //packetbuf_copyfrom(MESSAGE, strlen(MESSAGE));
  //mesh_send(&mesh, from);
}

const static struct mesh_callbacks callbacks = {recv, sent, timedout};
/*---------------------------------------------------------------------------*/
/* Set my Address 
/*---------------------------------------------------------------------------*/
static struct etimer et;
PROCESS_THREAD(assignment2, ev, data)
{  
  PROCESS_EXITHANDLER(mesh_close(&mesh);)
  PROCESS_BEGIN();
  mesh_open(&mesh, 132, &callbacks);
  
  //settings
  static int myAddress = 0;
  linkaddr_t addr;  
  addr.u8[0]=myAddress;
  addr.u8[1] = 0;
  
  SENSORS_ACTIVATE(button_sensor);

  while(1) {    
  	    
    PROCESS_WAIT_EVENT();
    if(ev == sensors_event && data == &button_sensor){
		myAddress+=1;
		addr.u8[0] = myAddress;
		addr.u8[1] = 0;
		printf("My Address: %d.%d\n", addr.u8[0],addr.u8[1]);
		uint16_t shortaddr = (addr.u8[0] << 8) + addr.u8[1];
		cc2420_set_pan_addr(IEEE802154_PANID, shortaddr, NULL);
		//And of course
		linkaddr_set_node_addr (&addr);
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/* Temperature sensing
/*---------------------------------------------------------------------------*/
PROCESS_THREAD (temp_process, ev, data)
{
  PROCESS_BEGIN ();
 
  {
    int16_t  tempint;
    uint16_t tempfrac;
    int16_t  raw;
    uint16_t absraw;
    int16_t  sign;
    char     minus = ' ';
    linkaddr_t addr_send;
 
    tmp102_init();
 
    while (1)
    {
      etimer_set(&et, TMP102_READ_INTERVAL);          // Set the timer
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));  // wait for its expiration

      sign = 1;

	    raw = tmp102_read_temp_raw();  // Reading from the sensor

      absraw = raw;
      if (raw < 0) { // Perform 2C's if sensor returned negative data
        absraw = (raw ^ 0xFFFF) + 1;
        sign = -1;
      }
	    tempint  = (absraw >> 8) * sign;
      tempfrac = ((absraw>>4) % 16) * 625; // Info in 1/10000 of degree
      minus = ((tempint == 0) & (sign == -1)) ? '-'  : ' ' ;
	    printf ("Temp = %c%d.%04d\n", minus, tempint, tempfrac);

		  //TODO: Send temps

		  //send temp		
		  //packetbuf_copyfrom(MESSAGE, strlen(MESSAGE));
      packetbuf_copyfrom(1, 2);
	    addr_send.u8[0] = 3;
	    addr_send.u8[1] = 0;
	    mesh_send(&mesh, &addr_send);


    }
  }
  PROCESS_END ();
}
/*---------------------------------------------------------------------------*/