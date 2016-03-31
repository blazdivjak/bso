#include "contiki.h"
#include "net/rime/rime.h"
#include "net/rime/mesh.h"

#include "dev/button-sensor.h"

#include "dev/leds.h"

#include <stdio.h>
#include <string.h>

#define FROM_MOTE_ADDRESS 1
#define TO_MOTE_ADDRESS 10

static uint8_t packetIndex = 0;
static int myAddress = 0;

static struct mesh_conn mesh;
/*---------------------------------------------------------------------------*/
PROCESS(example_mesh_process, "Mesh example");
AUTOSTART_PROCESSES(&example_mesh_process);
/*---------------------------------------------------------------------------*/
static void
sent(struct mesh_conn *c)
{
  // printf("packet sent\n");
}

static void
timedout(struct mesh_conn *c)
{
  // printf("packet timedout\n");
}

static void
recv(struct mesh_conn *c, const linkaddr_t *from, uint8_t hops)
{
  // printf("Data received from %d.%d; len=%d; hops=%d \t", from->u8[0], from->u8[1], packetbuf_datalen(), hops);
  printf("Hops=%d\n", hops);
  int idx = (uint8_t) *((char *)packetbuf_dataptr());
  if (myAddress == TO_MOTE_ADDRESS) { // if this is node 10
    printf("Received message ID: %i\n", idx);
    char msg[1]={idx};
    packetbuf_copyfrom(msg, 1);
    mesh_send(&mesh, from);
  } else if (myAddress == FROM_MOTE_ADDRESS) { // if this is node 1
      printf("Confirmation for %i\n", idx);
  }

}

const static struct mesh_callbacks callbacks = {recv, sent, timedout};
static struct etimer et;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_mesh_process, ev, data)
{
  PROCESS_EXITHANDLER(mesh_close(&mesh);)
  PROCESS_BEGIN();

  mesh_open(&mesh, 132, &callbacks);

  myAddress = linkaddr_node_addr.u8[0];

  while(1) {
    linkaddr_t addr;

    etimer_set(&et, CLOCK_SECOND);          // Set the timer
    PROCESS_WAIT_EVENT();

    if(myAddress==FROM_MOTE_ADDRESS && etimer_expired(&et)){
      packetIndex++;
      printf("Sending packet: %i\n", packetIndex);

      char msg[1]={packetIndex};
      packetbuf_copyfrom(msg, 1);
      addr.u8[0] = TO_MOTE_ADDRESS;
      addr.u8[1] = 0;
      mesh_send(&mesh, &addr);
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
