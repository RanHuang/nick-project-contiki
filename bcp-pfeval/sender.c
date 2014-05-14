#include "contiki.h"
#include "project-conf.h"
//#include "net/rime/rime.h"
#include "bcp.h"
#include "bcp_queue.h"
#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"
#include "lib/random.h"

#define RANDWAIT (random_rand() % SEND_INTERVAL)
static struct bcp_conn bcp;
//static rimeaddr_t addr;

PROCESS(sender_process, "Sender process");
AUTOSTART_PROCESSES(&sender_process);
/**********************************************************************/
void send_packet(void *v)
{
    static uint16_t send_num = 0;
    PRINTF("Call send function.\n");
    packetbuf_copyfrom("Hi",2);
    bcp_send(&bcp);
    send_num ++;
}
/**********************************************************************/
void sent_bcp(struct bcp_conn *c)
{
    PRINTF("SENT callback.\n");
    PRINTF("data='%s' length = %d\n",
                (char*)packetbuf_dataptr(),
                packetbuf_datalen());
}
/**********************************************************************/
static const struct bcp_callbacks bcp_callbacks = {NULL,sent_bcp,NULL};
/**********************************************************************/
PROCESS_THREAD(sender_process, ev, data)
{
    static struct ctimer timer_send_data;
    static struct etimer timer_period;

    PROCESS_BEGIN();
    bcp_open(&bcp, 146, &bcp_callbacks);
//    bcp_set_sink(&bcp, true);
    
    etimer_set(&timer_period, SEND_INTERVAL);
    while(1)
    {
        if(etimer_expired(&timer_period))
        {
            etimer_reset(&timer_period);
            ctimer_set(&timer_send_data, RANDWAIT, send_packet, NULL);
        }


    }

    PROCESS_END();
}


