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
#define FROM_MOTE_ADDRESS 1
#define TO_MOTE_ADDRESS 3
#define MESSAGE "Hello"
#define QUEUE_SIZE 256


/*
Message structure
*/
struct message{
    char id;
    char message;
};

/*Static values*/
static struct mesh_conn mesh;
struct message message_queue[QUEUE_SIZE];
//packet queue
static uint8_t packetIndex = 0;
static int myAddress = 0;

/*---------------------------------------------------------------------------*/
PROCESS(assignment2, "Assignment2 proces");
AUTOSTART_PROCESSES(&assignment2);
/*---------------------------------------------------------------------------*/

static void sent(struct mesh_conn *c)
{
  printf("packet sent\n");

  //remove buffered value

}

static void timedout(struct mesh_conn *c)
{
  printf("packet timedout\n");
  //printf("queue length: %d",  packetqueue_len(&queue));
  //add value to buffer and try to send it next time

}

static uint8_t decode_temp(char *msg){
  // prepare variables for conversion of received temperature
  int16_t  tempint;
  uint16_t tempfrac;
  int16_t  raw;
  uint16_t absraw;
  int16_t  sign = 1;
  char     minus = ' ';
  uint8_t  index = 0;
  // convert received temperatura and index
  char *rcv_data = msg;
  raw = ((rcv_data[1] & 0xff)<<8) + (rcv_data[2] & 0xff);
  index = rcv_data[0];
  absraw = raw;
  if (raw < 0) { // Perform 2C's if sensor returned negative data
    absraw = (raw ^ 0xFFFF) + 1;
    sign = -1;
  }
  tempint  = (absraw >> 8) * sign;
  tempfrac = ((absraw>>4) % 16) * 625; // Info in 1/10000 of degree
  minus = ((tempint == 0) & (sign == -1)) ? '-'  : ' ' ;
  printf ("Index: %3d - received temp = %c%d.%04d\n", index, minus, tempint, tempfrac);
  return index;
}

static void recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops){
  printf("Data received from %d.%d: %d bytes\n",
  from->u8[0], from->u8[1], packetbuf_datalen(), packetbuf_datalen());

  if (myAddress == FROM_MOTE_ADDRESS){
    // receive ACK and clear queue

  } else if (myAddress == TO_MOTE_ADDRESS){
    // receive data, print to STD_OUT and send ACK
    int i;
    for(i=0; i<packetbuf_datalen(); i+=3){
      uint8_t index_to_confirm = decode_temp(((char *)packetbuf_dataptr()) + i);
    }

  }
  
  //packetbuf_copyfrom(MESSAGE, strlen(MESSAGE));
  //mesh_send(&mesh, from);
}

/*---------------------------------------------------------------------------*/
// callbacks for mesh (must be declared after declaration)
const static struct mesh_callbacks callbacks = {recv, sent, timedout};
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Send temperature to 3.0 */
/*---------------------------------------------------------------------------*/
static void send_temperature(){
  //temperature sensing
  int16_t  tempint;
  uint16_t tempfrac;
  int16_t  raw;
  uint16_t absraw;
  int16_t  sign = 1;
  char     minus = ' ';
  linkaddr_t addr_send; 
  tmp102_init();
  
  // Reading from the sensor
  raw = tmp102_read_temp_raw();
  absraw = raw;
  if (raw < 0) { // Perform 2C's if sensor returned negative data
    absraw = (raw ^ 0xFFFF) + 1;
    sign = -1;
  }
  tempint  = (absraw >> 8) * sign;
  tempfrac = ((absraw>>4) % 16) * 625; // Info in 1/10000 of degree
  minus = ((tempint == 0) & (sign == -1)) ? '-'  : ' ' ;

  //Save message to queue
  char msg[2] = {((raw & 0xff00)>>8), (raw&0xff)};  
  struct message m;
  m.id=packetIndex;
  m.message=msg;
  message_queue[packetIndex]=m;
  int8_t message_size = 3;

  char msg_with_index[6]={packetIndex, msg[0], msg[1],0,0,0};//,i,queue[i][0],queue[i][1]};    

  //Create message and piggy back unsent messages
  int i = 0;
  
  for(i;i<QUEUE_SIZE;i++){
    if (message_queue[i].message!=NULL){
          message_size=6;
          msg_with_index[3]=message_queue[i].id;
          msg_with_index[4]=message_queue[i].message[0];
          msg_with_index[5]=message_queue[i].message[1];
    }
  }

  //Increment packet index
  packetIndex++;

  //Our messurment
  printf ("Index: %3d - temp off sensor = %c%d.%04d\n", packetIndex, minus, tempint, tempfrac);

  //send temp
  packetbuf_copyfrom(msg_with_index, message_size);     
  addr_send.u8[0] = TO_MOTE_ADDRESS;
  addr_send.u8[1] = 0;
  printf("Mesh status: %d\n", mesh_ready(&mesh));
  mesh_send(&mesh, &addr_send);
}

/*---------------------------------------------------------------------------*/
/* Increase MOTE Z jedan Address */
/*---------------------------------------------------------------------------*/
static void increase_address(){  
  myAddress+=1;
  linkaddr_t addr;  
  addr.u8[0] = myAddress;
  addr.u8[1] = 0;
  printf("My Address: %d.%d\n", addr.u8[0],addr.u8[1]);
  uint16_t shortaddr = (addr.u8[0] << 8) + addr.u8[1];
  cc2420_set_pan_addr(IEEE802154_PANID, shortaddr, NULL);   
  linkaddr_set_node_addr (&addr);
}

/*---------------------------------------------------------------------------*/
/* Set my Address and sense temperature */
/*---------------------------------------------------------------------------*/
static struct etimer et;
PROCESS_THREAD(assignment2, ev, data)
{  
  PROCESS_EXITHANDLER(mesh_close(&mesh);)
  PROCESS_BEGIN();  
  mesh_open(&mesh, 132, &callbacks);  
  
  SENSORS_ACTIVATE(button_sensor);

  // set address
  increase_address();

  while(1) {    

    etimer_set(&et, TMP102_READ_INTERVAL);          // Set the timer    
    PROCESS_WAIT_EVENT();
    
    //set address
    if(ev == sensors_event && data == &button_sensor){
      increase_address();
    }
    //sense temperature and send it
    if(myAddress==FROM_MOTE_ADDRESS && etimer_expired(&et)){
      send_temperature();
    }

  }
  PROCESS_END();
}
