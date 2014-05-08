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
static void recv_bcp(struct bcp_conn *c, rimeaddr_t *from)
{
    static uint16_t recv_num = 0;
    PRINTF("RECV callback.");
    PRINTF("'%s' from node[%d].[%d];counter=%d\n",
                (char *)packetbuf_dataptr(),
                from->u8[0],
                from->u8[1],
                ++recv_num);
}
/**************************************************************************/
static const struct bcp_callbacks bcp_callbacks = {recv_bcp};
PROCESS_THREAD(sink_process, ev, data)
{
    PROCESS_BEGIN();

    bcp_open(&bcp, 146, &bcp_callbacks);
    bcp_set_sink(&bcp, true);

    PROCESS_END();
}
