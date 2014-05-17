#include "contiki.h"
#include "net/uip.h"
#include "rpl/rpl.h"
#include "net/netstack.h"

#include "project-conf.h"
#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"

#define UIP_IP_BUF ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
static struct uip_udp_conn *sink_conn;

PROCESS(sink_process, " UDP sink process");
AUTOSTART_PROCESSES(&sink_process);

/************************************************************************/
static void print_local_addresses(void)
{
    int i;
    uint8_t state;

    PRINTF("Sink IPV6 addresses: ");
    for(i = 0; i < UIP_DS6_ADDR_NB; i++)
    {
        state = uip_ds6_if.addr_list[i].state;
        if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED)
        {
            PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
            PRINTF("\n");
            /* hack to make the address "final" ?why?*/
            if(state == ADDR_TENTATIVE)
              uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
        }
    }
}
/************************************************************************/
/*
static void tcpip_handler(void)
{
    char *appdata;
    if(uip_newdata())
    {
        appdata = (char *)uip_appdata;
        appdata[uip_datalen()] = '\0';
        printf("DATA recv '%s'\n FROM ", appdata);
        PRINTF("%d", 
            UIP_IP_BUF->srcipaddr.u8[sizeof(UIP_IP_BUF->srcipaddr.u8) -1] );
        PRINTF("\n");
        // TO DO : send the REPLAY message

    }
}*/
/************************************************************************/
static void tcpip_handler(void)
{
    struct app_msg *appdata;
    rimeaddr_t sender;
    uint16_t seqno;
    uint16_t hops;

    if(uip_newdata())
    {
        appdata = (struct app_msg*) uip_appdata;
        seqno = appdata->seqno;
        hops = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1;
        sender.u8[0] = UIP_IP_BUF->srcipaddr.u8[15];
        sender.u8[1] = UIP_IP_BUF->srcipaddr.u8[15];

        PRINTF("DATA received from %d NO %d\n",
            UIP_IP_BUF->srcipaddr.u8[sizeof(UIP_IP_BUF->srcipaddr.u8) - 1],
            seqno);

        PRINTF("PayloadLen %u\n", 8 + uip_datalen() / 2);
        PRINTF("ori seq hop:%u %u %u\n", sender.u8[0] + (sender.u8[1]<<8),
                        seqno, hops);
        PRINTF("HOPS %u\n", hops);
        // TO DO : send the REPLAY message
    }
}

/***********************************************************************/
PROCESS_THREAD(sink_process, ev, data)
{
    uip_ipaddr_t ipaddr;
    struct uip_ds6_addr *root_if;

    PROCESS_BEGIN();

//    PROCESS_PAUSE();

    PRINTF("The sink process begins.\n");
    /* Mode 2 - 16 bits inline *//* Mode 2 is OK. Mode 3 is not. Why??*/
    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);

    uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);
    root_if = uip_ds6_addr_lookup(&ipaddr);
    if(root_if != NULL)
    {
        rpl_dag_t *dag;
        dag = rpl_set_root(RPL_DEFAULT_INSTANCE, (uip_ip6addr_t *)&ipaddr);
        uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
        rpl_set_prefix(dag, &ipaddr, 64);
        PRINTF("created a new RPL dag\n");
    } else {
        PRINTF("failed to creat a new RPL DAG\n");
    }

    print_local_addresses();

    /* The data sink runs with a 100% duty cycle in order to ensure high
       packet reception rates. */
    NETSTACK_MAC.off(1);

    sink_conn = udp_new(NULL, UIP_HTONS(UDP_SENDER_PORT), NULL);
    if(sink_conn == NULL)
    {
        PRINTF("No UDP connection available, exiting the process!\n");
        PROCESS_EXIT();
    }
    udp_bind(sink_conn, UIP_HTONS(UDP_SINK_PORT));

    PRINTF("Created a sink connection with remote address ");
    PRINT6ADDR(&sink_conn->ripaddr);
    PRINTF(" local/remote port %u/%u\n", UIP_HTONS(sink_conn->lport),
                UIP_HTONS(sink_conn->rport));

    while(1)
    {
        PROCESS_YIELD();
        if(ev == tcpip_event)
        {
            PRINTF("New data arrived!\n");
            tcpip_handler();
        }
    }



    PROCESS_END();
}
