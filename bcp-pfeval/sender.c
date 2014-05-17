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
    static uint16_t send_num = 1;
    struct app_msg msg;
    msg.seqno = send_num;
    memcpy(msg.data, "Hi BCP.", sizeof(msg.data) - 2);
    PRINTF("SEND pakcet NO %u\n", send_num);
    packetbuf_copyfrom(&msg, sizeof(msg));
    bcp_send(&bcp);
    send_num ++;
}
/**********************************************************************/
void sent_bcp(struct bcp_conn *c, uint8_t backpressure)
{
    struct app_msg *msg = (struct app_msg *)packetbuf_dataptr();
    PRINTF("SENT callback: data='%s' length = %d backlog= %u\n",
                msg->data,
                packetbuf_datalen(),
                backpressure);
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
        PROCESS_YIELD();

        if(etimer_expired(&timer_period))
        {
            etimer_reset(&timer_period);
            ctimer_set(&timer_send_data, RANDWAIT, send_packet, NULL);
        }


    }

    PROCESS_END();
}


