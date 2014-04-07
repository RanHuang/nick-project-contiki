#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/uip.h"
#include "net/rpl/rpl.h"
#include "net/rime/rimeaddr.h"
#include "net/netstack.h"
#include "dev/button-sensor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "collect-view.h"

#include "project-conf.h"

#define UIP_IP_BUF ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

static struct uip_udp_conn *sink_conn;
PROCESS(udp_sink_process, "UDP sink process");
AUTOSTART_PROCESSES(&udp_sink_process);
/*-----------------------------------------------------------------------*/
void collect_common_net_print(void)
{
	printf("I am a sink!\n");
}
/*-----------------------------------------------------------------------*/
static unsigned short get_time(void)
{
//	return clock_seconds();
	return clock_time();
}
/*-----------------------------------------------------------------------*/
void collect_common_recv(const rimeaddr_t *originator, uint8_t seqno,
			             uint8_t hops,
						 uint8_t *payload, uint16_t payload_len)
{
	uint16_t time_receive, time_send ,delay_time;
	struct collect_view_data_msg msg;

	printf("PayloadLen:");
	printf("%u",8 + payload_len / 2); 

	time_receive = get_time();
	printf(" Received Time: %u ", time_receive);

//	printf("%lu %lu",
//				((time_receive >> 16) & 0xffff), time_receive & 0xffff);
	memcpy(&msg, payload, sizeof(msg));
	time_send = msg.clock ;
	printf("Send time: %u ", time_send);
//	printf(" Send Time: %lu %lu ",
//				((time_send >> 16) & 0xffff), time_send & 0xffff);
	
//	PRINTF(" RECEIVE %lu SEND %lu \n", time_receive, time_send);
	printf(" ori seq hop:");
	printf("%u %u %u\n", originator->u8[0] + (originator->u8[1] << 8), 
						seqno, hops);
	PRINTF("HOPS %u \n", hops);
/*	printf(" data:");
	for(i = 0; i < payload_len / 2; i++)
	{   
		memcpy(&data, payload, sizeof(data));
		payload += sizeof(data);
		printf(" %u", data);
	}   
	*/
//	printf("\nThe size of received msg = %u", payload_len + 16);

	printf("\n");

	delay_time = time_receive - time_send;
	PRINTF("DELAYTIME %u \n",  
				((time_receive < time_send) ? 
				(time_receive + 65536 - time_send) : delay_time));
}
/*-----------------------------------------------------------------------*/
static void tcpip_handler(void)
{
	uint8_t *appdata;
	rimeaddr_t sender;
	uint8_t seqno;
	uint8_t hops;

	if(uip_newdata()) {
	
		PRINTF("DATA Received from %d\n",
			UIP_IP_BUF->srcipaddr.u8[sizeof(UIP_IP_BUF->srcipaddr.u8) - 1]);

		appdata = (uint8_t *)uip_appdata;
		sender.u8[0] = UIP_IP_BUF->srcipaddr.u8[15];
		sender.u8[1] = UIP_IP_BUF->srcipaddr.u8[14];
		seqno = *appdata;
		hops = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1;
		collect_common_recv(&sender, seqno, hops, 
							appdata + 2, uip_datalen() - 2);
	}
}
/*-----------------------------------------------------------------------*/
static void print_local_addresses(void)
{
	int i;
	uint8_t state;

	PRINTF("Sink IPv6 addresses: ");
	for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
		state = uip_ds6_if.addr_list[i].state;
		if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
			PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
			PRINTF("\n");
			if(state == ADDR_TENTATIVE) {
				uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
			}
		}
	}
}
/*-----------------------------------------------------------------------*/
PROCESS_THREAD(udp_sink_process, ev, data)
{
	uip_ipaddr_t ipaddr;
	struct uip_ds6_addr *root_if;
	PROCESS_BEGIN();
	PROCESS_PAUSE();
	SENSORS_ACTIVATE(button_sensor);
	PRINTF("UDP sink started\n");

#if UIP_CONF_ROUTER
	uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
	uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);
	root_if = uip_ds6_addr_lookup(&ipaddr);
	if(root_if != NULL) {
		rpl_dag_t *dag;
		dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)&ipaddr);
		uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
		rpl_set_prefix(dag, &ipaddr, 64);
		PRINTF("Created a new RPL dag\n");
	} else {
		PRINTF("failed to create a new RPL dag\n");
	}
#endif /* UIP_CONF_ROUTER */
	print_local_addresses();

	NETSTACK_RDC.off(1);

	sink_conn = udp_new(NULL, UIP_HTONS(UDP_SENDER_PORT), NULL);
	udp_bind(sink_conn, UIP_HTONS(UDP_SINK_PORT));

	PRINTF("Created a sink connection with remote address ");
	PRINT6ADDR(&sink_conn->ripaddr);
	PRINTF(" local/remote port %u/%u\n", UIP_HTONS(sink_conn->lport),
			UIP_HTONS(sink_conn->rport));

	while(1) {
		PROCESS_YIELD();
		if(ev == tcpip_event) {
//			collect_common_net_print();
			tcpip_handler();
		} else if(ev == sensors_event && data == &button_sensor) {
			PRINTF("Initiating global repair\n");
			rpl_repair_root(RPL_DEFAULT_INSTANCE);
		}
	}
	PROCESS_END();
}
/*-----------------------------------------------------------------------*/
