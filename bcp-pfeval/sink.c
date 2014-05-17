#include "contiki.h"
#include "bcp.h"
#include "bcp_queue.h"
#include "net/rime.h"

#include "project-conf.h"
#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"

static struct bcp_conn bcp;
/************************************************************************/
PROCESS(sink_process, "Sink process");
AUTOSTART_PROCESSES(&sink_process);
/************************************************************************/
static void recv_bcp(struct bcp_conn *c, rimeaddr_t *from, uint8_t hopCount)
{
    static uint16_t recv_num = 0;
    struct app_msg *msg = (struct app_msg *)packetbuf_dataptr();

    PRINTF("RECV callback: '%s' from node[ %d ].[ %d ];NO=%u;counter=%u\n",
                msg->data,
                from->u8[0],
                from->u8[1],
                msg->seqno,
                ++recv_num);
    PRINTF("HOPS = %u\n", hopCount);
}
/**************************************************************************/
static const struct bcp_callbacks bcp_callbacks = {recv_bcp, NULL, NULL};
PROCESS_THREAD(sink_process, ev, data)
{
    PROCESS_BEGIN();

    bcp_open(&bcp, 146, &bcp_callbacks);
    bcp_set_sink(&bcp, true);

    PROCESS_END();
}
