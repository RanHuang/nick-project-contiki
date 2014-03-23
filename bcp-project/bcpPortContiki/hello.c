/******************************************************************************
* THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.

Institute:  University of Southern California
Group:      Autonomous Networks Research Group
URL:        http://anrg.usc.edu
License:    BSD
Contiki Version:    2.6
Intended Platforms: Tmote Sky
Tested Platforms:   Tmote Sky
Description: Port of the Backpressure Collection Protocol (BCP) from TinyOS to
             Contiki. The initial implementation of BCP was done in TinyOS. We
             took this code base and ported it over to the Contiki OS. This
             project is a preliminary step towards creating a native
             implementation of BCP in the Contiki environment.
Contributors to this version: Nicolas Tisa-Leonard <tisaleon@usc.edu>
                              Juan Gutierrez <jmgutier@usc.edu>
                              Suvil Deora <deora@usc.edu>
                              Students conducting research at the Autonomous
                              Networks Research Group
******************************************************************************/

#include "contiki.h"
#include "net/rime.h"
#include "BcpRoutingEngine.h"
#include "BcpForwardingEngine.h"
#include "Bcp.h"
#include <stdio.h>
#include "mydebug.h"
#include "cc2420.h"

#define PACKETS_TO_SEND 500

/*
msglevel  0: mute all messages (even errors)
         10: display only errors
         100: unicast stuff and evething above
         200: breadcrumbs and warnings and everthing above
*/
int32_t msglevel = 0;

static message_wrapper_t*

recv_uc(message_wrapper_t* msg)
{
	static uint16_t received_packets[20];

	//Packet Count
	// printf("**********************************************************************\n");
	// printf("Root Received Message Originating from = %d.%d\n",  msg -> bcp_data_header.origin.u8[0], msg -> bcp_data_header.origin.u8[1]);
	// printf("Number of Hops = %d\n", msg -> bcp_data_header.hopCount);
	printf("Total Received Packets from %d.%d = %d\n", 
		msg -> bcp_data_header.origin.u8[0], 
		msg -> bcp_data_header.origin.u8[1],
		++received_packets[msg -> bcp_data_header.origin.u8[0]]);
	// printf("**********************************************************************\n");

	//End to End Delay
	printf("Received Packet from: %d.%d with SequenceNum = %lu\n", msg -> bcp_data_header.origin.u8[0], msg -> bcp_data_header.origin.u8[1],
		msg -> bcp_data_header.packetSeqNum);

	return msg;
}

/*---------------------------------------------------------------------------*/
PROCESS(hello_process, "Hello process");
AUTOSTART_PROCESSES(&hello_process);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(hello_process, ev, data)
{
	PROCESS_BEGIN();

	//Values for address/timer/messages
  	static rimeaddr_t addr;
  	static struct etimer et;
  	static struct etimer warm_up_et;
  	static message_wrapper_t message;
  	static packet_sent_count = 0;

    cc2420_set_txpower(15);

    //Initialize the routing and forwarding engines
	bcpRoutingEngine_Init();
	bcp_forwarding_engine_init();

	// //Start the routing and forwarding engine
	bcpRoutingEngine_stdControlStart();
	bcpForwardingEngine_stdControlStart();

    addr.u8[0] = 1;
    addr.u8[1] = 0;

	//If NodeID == 1.0, set as root and never send any messages
	if(rimeaddr_cmp(&addr, &rimeaddr_node_addr)) {
		rootControl_setRoot();
		register_cb_message_receive(recv_uc);
	}
	else {
		//Allow other nodes to start up
		etimer_set(&warm_up_et, CLOCK_SECOND * 10);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&warm_up_et));

		//Start timer to send packets every X seconds
		while(1) {
			etimer_set(&et, CLOCK_SECOND * 2);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
			if(packet_sent_count < PACKETS_TO_SEND) {
				pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
				message.bcp_data_header.packetSeqNum = packet_sent_count;
				forwardingEngine_Send(&message, 0);
				++packet_sent_count;
			}
		}	
	}

	while(1) {
		PROCESS_WAIT_EVENT();
	}


	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
