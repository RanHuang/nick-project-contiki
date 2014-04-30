#include "contiki.h"
#include "project-conf.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "sys/ctimer.h"
#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"
#include "lib/random.h"
#include "net/uip-udp-packet.h"
#include "net/rpl/rpl.h"
#include <string.h>

#define RANDWAIT (random_rand() % SEND_INTERVAL)

static struct uip_udp_conn *sender_conn;
static uip_ipaddr_t sink_ipaddr;

PROCESS(sender_process, "UDP sender process");
AUTOSTART_PROCESSES(&sender_process);
/************************************************************************/
static void set_global_address(void)
{
    uip_ipaddr_t ipaddr;

    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
    /*where is the uip_lladdr defined?*/
    uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
    /* Mode 2 - 16 bits inline */
    /* Set sink address. What if more than one sink? */
    uip_ip6addr(&sink_ipaddr, 0xaaaa, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);
}
/************************************************************************/
static void print_local_addresses(void)
{
    int i;
    uint8_t state;

    PRINTF("Sender IPV6 addresses: ");
    for(i = 0; i < UIP_DS6_ADDR_NB; i++)
    {
        state = uip_ds6_if.addr_list[i].state;
        if(uip_ds6_if.addr_list[i].isused &&
                    (state == ADDR_TENTATIVE || state == ADDR_PREFERRED))
        {
            PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
            PRINTF("**print_local_addresses**\n");
            /* hack to make the address "final" ?why?*/
            if(state == ADDR_TENTATIVE)
              uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
        }
    }
}
/************************************************************************/
static void tcpip_handler(void)
{
    char *str;
    if(uip_newdata())
    {
        str = uip_appdata;
        str[uip_datalen()] = '\0';
        printf("DATA recv '%s'\n", str);
    }
}
/***********************************************************************/
/*
static void send_packet(void *ptr)
{
    static int seq_id;
    char buf[MAX_PAYLOAD_LEN];

    seq_id++;
    PRINTF("DATA send to %d 'Hello %d'\n",
                sink_ipaddr.u8[sizeof(sink_ipaddr.u8) - 1], seq_id);
    PRINTF("aDAT SEND\n");
    sprintf(buf, "Hello %d from the sender", seq_id);
    uip_udp_packet_sendto(sender_conn, buf, strlen(buf), &sink_ipaddr,
                            UIP_HTONS(UDP_SINK_PORT));
}*/
/***********************************************************************/
static void send_packet(void *ptr)
{
    static uint16_t seqno = 0;
    struct app_msg msg;

    uint16_t parent_etx;
    uint16_t rtmetric;
    uint16_t num_neighbors;
    uint16_t beacon_interval;
    rpl_parent_t *preferred_parent;
    rimeaddr_t parent;
    rpl_dag_t *dag;

    if(sender_conn == NULL) return;

    memset(&msg, 0, sizeof(msg));

    seqno++;
    msg.seqno = seqno;

    rimeaddr_copy(&parent, &rimeaddr_null);
    parent_etx = 0;
    // Let's suppose we have only one instance. 
    dag = rpl_get_any_dag();
    if( dag != NULL)
    {
        preferred_parent = dag->preferred_parent;
        if(preferred_parent != NULL)
        {
            uip_ds6_nbr_t *nbr;
            nbr = uip_ds6_nbr_lookup(&preferred_parent->next);
            if(nbr != NULL)
            {
                // Use parts of the ipv6 address as the parent address.
                  // In reversed byte order. 
                parent.u8[RIMEADDR_SIZE - 1] =
                    nbr->ipaddr.u8[sizeof(uip_ipaddr_t) - 2];
                parent.u8[RIMEADDR_SIZE - 2] = 
                    nbr->ipaddr.u8[sizeof(uip_ipaddr_t) - 1];
                parent_etx =
                    rpl_get_parent_rank((rimeaddr_t *)uip_ds6_nbr_get_ll(nbr)) / 2;
            }
        }
        rtmetric = dag->rank;
        beacon_interval =
            (uint16_t)((2L<<dag->instance->dio_intcurrent) / 1000);
        num_neighbors = RPL_PARENT_COUNT(dag);

    } else {
        rtmetric = 0;
        beacon_interval = 0;
        num_neighbors = 0;
    }
  
    msg.parent_etx = parent_etx;
    msg.rtmetric = rtmetric;
    msg.num_neighbors = num_neighbors;
    msg.beacon_interval = beacon_interval;
    memset(msg.data, 11, sizeof(msg.data) - 2);
    msg.data[10] = seqno;
    uip_udp_packet_sendto(sender_conn, &msg, sizeof(msg),
                           &sink_ipaddr, UIP_HTONS(UDP_SINK_PORT));
    PRINTF("DATA send NO %d to %d * msg size %u\n", seqno,
                sink_ipaddr.u8[sizeof(sink_ipaddr.u8) - 1], sizeof(msg));
                
}
/***********************************************************************/
PROCESS_THREAD(sender_process, ev, data)
{
    
    static struct etimer periodic;
    static struct ctimer backoff_timer;

    PROCESS_BEGIN();

//    PROCESS_PAUSE();

    set_global_address();
    
    PRINTF("The sender process begins.\n");
    
    print_local_addresses();
    /* new connection with remote host */
    sender_conn = udp_new(NULL, UIP_HTONS(UDP_SINK_PORT), NULL);
    if(sender_conn == NULL)
    {
        PRINTF("No UDP connection available, exiting the process!\n");
        PROCESS_EXIT();
    }
    udp_bind(sender_conn, UIP_HTONS(UDP_SENDER_PORT));
    PRINTF("Created a connection with the server ");
    PRINT6ADDR(&sender_conn->ripaddr);
    PRINTF(" local/remote port %u/%u\n", UIP_HTONS(sender_conn->lport),
                UIP_HTONS(sender_conn->rport));

    etimer_set(&periodic, SEND_INTERVAL);
    while(1) {
        PROCESS_YIELD();
        if(ev == tcpip_event)
          tcpip_handler();

        if(etimer_expired(&periodic))
        {
            etimer_reset(&periodic);
            ctimer_set(&backoff_timer, RANDWAIT, send_packet, NULL);
        }
    }

    PROCESS_END();
}
