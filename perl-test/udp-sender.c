#include "contiki.h"
#include "lib/random.h"
#include "net/netstack.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/uip-udp-packet.h"
#include "net/neighbor-info.h"
#include "net/rpl/rpl.h"
#include "collect-view.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "project-conf.h"

static struct uip_udp_conn *sender_conn;
static uip_ipaddr_t sink_ipaddr;
/*-----------------------------------------------------------------------*/
PROCESS(udp_sender_process, "UDP sender process");
AUTOSTART_PROCESSES(&udp_sender_process);
/*-----------------------------------------------------------------------*/
extern uip_ds6_route_t uip_ds6_routing_table[UIP_DS6_ROUTE_NB];
void collect_common_net_print(void)
{
	rpl_dag_t *dag;
	int i;
	/* Let's suppose we have only one instance. */
	dag = rpl_get_any_dag();
	if(dag->preferred_parent != NULL) {
		PRINTF("Preferred parent: ");
		PRINT6ADDR(&uip_ds6_routing_table[i].ipaddr);
		PRINTF("\n");
	}
	PRINTF("Route entries:\n");
	for(i = 0; i < UIP_DS6_ROUTE_NB; i++) {
		if(uip_ds6_routing_table[i].isused) {
			PRINT6ADDR(&uip_ds6_routing_table[i].ipaddr);
			PRINTF("\n");
		}
	}
	PRINTF("---\n");
}
/*-----------------------------------------------------------------------*/
void collect_common_send(void)
{
	static uint8_t seqno;
	struct {
		uint8_t seqno;
		uint8_t for_alignment;
		struct collect_view_data_msg msg;
		uint8_t hop_count;
		uint8_t lifetime;
		uint16_t data[72];
	} msg;
	
	uint16_t parent_etx;
	uint16_t rtmetric;
	uint16_t num_neighbors;
	uint16_t beacon_interval;
	rpl_parent_t *preferred_parent;
	rimeaddr_t parent;
	rpl_dag_t *dag;

	if(sender_conn == NULL) {
		return;
	}
	memset(&msg, 0, sizeof(msg));
	seqno++;
	if(seqno == 0) {
		seqno = 128; /* Wrap to 128 to identify restarts */
	}
	msg.seqno = seqno;

	rimeaddr_copy(&parent, &rimeaddr_null);
	parent_etx = 0;

	/* Let's suppose we have only one instance. */
	dag = rpl_get_any_dag();
	if(dag != NULL) {
		preferred_parent = dag->preferred_parent;
		if(preferred_parent != NULL) {
			uip_ds6_nbr_t *nbr;
			nbr = uip_ds6_nbr_lookup(&preferred_parent->addr);
			if(nbr != NULL) {
				/* Use parts of the IPv6 address as the parent address,
				 * in reversed byte order. */
				parent.u8[RIMEADDR_SIZE - 1] = 
					nbr->ipaddr.u8[sizeof(uip_ipaddr_t) - 2];
				parent.u8[RIMEADDR_SIZE - 2] =
					nbr->ipaddr.u8[sizeof(uip_ipaddr_t) - 1];
				parent_etx = 
				    neighbor_info_get_metric((rimeaddr_t *)&nbr->lladdr)/2;
			}
		}

		rtmetric = dag->rank;
		beacon_interval = 
			(uint16_t) ((2L << dag->instance->dio_intcurrent) / 1000);
		num_neighbors = RPL_PARENT_COUNT(dag);
	} else {
		rtmetric = 0;
		beacon_interval = 0;
		num_neighbors = 0;
	}
	
	collect_view_construct_message(&msg.msg, &parent, parent_etx, rtmetric,
									num_neighbors, beacon_interval);
	memset(msg.data, 12, sizeof(msg.data));
//	PRINTF("The size of the packet is: %d\n", sizeof(msg));
	PRINTF("DATA send NO %d to %d\n", seqno,
				sink_ipaddr.u8[sizeof(sink_ipaddr.u8) - 1]);
//	printf("Send a message: %u \n", seqno);
	uip_udp_packet_sendto(sender_conn, &msg, sizeof(msg),
						  &sink_ipaddr, UIP_HTONS(UDP_SINK_PORT));
//	PRINTF("The size of the packet is: %u\n", sizeof(msg));
}
/*------------------void collect_common_send(void)-----------------------*/
/*-----------------------------------------------------------------------*/
static void print_local_address(void)
{
	int i;
	uint8_t state;
	PRINTF("Sender IPv6 address: ");
	for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
		state = uip_ds6_if.addr_list[i].state;
		if(uip_ds6_if.addr_list[i].isused &&
			(state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
			PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
			PRINTF("\n");
			/* hack to make address "final" */
			if(state == ADDR_TENTATIVE) {
				uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
			}
		}
	}
}
/*-----------------------------------------------------------------------*/
static void set_global_address(void)
{
	uip_ipaddr_t ipaddr;
	uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
	uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
	uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
	/* set sink address */
	uip_ip6addr(&sink_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
}
/*-----------------------------------------------------------------------*/
PROCESS_THREAD(udp_sender_process, ev, data)
{
	static struct etimer period_timer, wait_timer;
	PROCESS_BEGIN();
	
	set_global_address();
	PRINTF("UDP sender process started\n");
	print_local_address();

	/* new connection with remote host */
	sender_conn = udp_new(NULL, UIP_HTONS(UDP_SINK_PORT), NULL);
	udp_bind(sender_conn, UIP_HTONS(UDP_SENDER_PORT));

	PRINTF("Created a connection with the sink ");
	PRINT6ADDR(&sender_conn->ripaddr);
	PRINTF(" local/remote port %u/%u\n",
			UIP_HTONS(sender_conn->lport), UIP_HTONS(sender_conn->rport));
	
	etimer_set(&period_timer, CLOCK_SECOND * PERIOD);
	while(1) {
		PROCESS_WAIT_EVENT();
		if(ev == PROCESS_EVENT_TIMER) {
			if(data == &period_timer) {
				etimer_reset(&period_timer);
				etimer_set(&wait_timer,
							random_rand() % (CLOCK_SECOND * RANDWAIT));
			} else if(data ==&wait_timer) {
				/* Time to send a data. */
				collect_common_send();
			}
		}
	}
	PROCESS_END();
}
/*-----------------------------------------------------------------------*/

