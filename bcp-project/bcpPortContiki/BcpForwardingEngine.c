#include "BcpForwardingEngine.h"
#include "BcpRoutingEngine.h"
#include <string.h>
#include <limits.h>


PROCESS(sendBeaconTask, "sendBeaconTask");
PROCESS(sendDataTask, "sendDataTask");
#ifndef BEACON_ONLY 
PROCESS(snoopHandlerTask, "snoopHandlerTask");
#endif

//Connections
static struct broadcast_conn broadcast;
static struct unicast_conn unicast;
/*---------------------------------------------------------------------------*/
//User defined callbacks
void register_cb_message_receive(event_message_receive function) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	ev_msg_receive = function;
}

void register_cb_send_done(event_send_done function) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	ev_send_done = function;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
//Broadcast callbacks
//These will either be packets not intended for us or beacons
static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);

	bool is_broadcast = rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                                &rimeaddr_null);

	if(is_broadcast) {
		pmesg(150, "beacon message received from %d.%d: '%s' ",
					from->u8[0], from->u8[1], (bcp_beacon_header_t *)packetbuf_dataptr());
		pmesg(150, "on channel: %d\n", c->c.channel.channelno);

		beaconReceive_receive((bcp_beacon_header_t*) packetbuf_dataptr());
	}

	#ifndef BEACON_ONLY 
	else {
		pmesg(150, "snooping message received from %d.%d: '%s' ",
					from->u8[0], from->u8[1], (message_wrapper_t *)packetbuf_dataptr());
		pmesg(150, "on channel: %d\n", c->c.channel.channelno);
		subSnoop_receive((message_wrapper_t*) packetbuf_dataptr());
	}
	#endif
}

//Beacon Sent
static void
broadcast_sent(struct broadcast_conn *ptr, int status, int num_tx)
{
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	pmesg(200, "beacon packet sent\n");

	beaconSend_sendDone((bcp_beacon_header_t*)packetbuf_dataptr());
}
/*---------------------------------------------------------------------------*/
//Unicast callbacks
//These will be packets intended for us
//Packet Received
static void
recv_uc(struct unicast_conn *c, const rimeaddr_t *from)
{
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	pmesg(100, "unicast message received from %d.%d\n",
			from->u8[0], from->u8[1]);

	subReceive_receive((message_wrapper_t*)packetbuf_dataptr());
}

//Packet Sent
static void
sent_uc(struct unicast_conn *ptr, int status, int num_tx)
{
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	pmesg(100, "unicast packet sent to %d.%d\n",
		((message_wrapper_t*)packetbuf_dataptr()) -> to.u8[0],
		((message_wrapper_t*)packetbuf_dataptr()) -> to.u8[1]);

	subSend_sendDone((message_wrapper_t*)packetbuf_dataptr(), status);
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv, broadcast_sent};
static const struct unicast_callbacks unicast_callbacks = {recv_uc, sent_uc};

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
error_t bcp_forwarding_engine_init() {
	//Initialize the queues, stacks, and pools
	// memb_init(&send_stack_mem);
	// memb_init(&snoop_queue_mem);
	// memb_init(&send_stack_mem);
	// memb_init(&send_stack_mem);

	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);

	list_init(send_stack);
	list_init(snoop_queue);
	list_init(q_entry_pool);
	list_init(message_pool);
	list_init(snoop_pool);

	//Initialize defaults
	beaconSending = false;
	extraBeaconSending = false;
	isRunningForwardingEngine = false;
	sending = false;
	radioOn = false;
	seqno = 0;
	nullSeqNo = 0;
	
	beaconHdr = &beaconMsgBuffer; //call BeaconSend.getPayload(&beaconMsgBuffer, call BeaconSend.maxPayloadLength());
	dataCRCSuccessCount = 0;
	dataCRCFailCount = 0;
	snoopCRCSuccessCount = 0;
	snoopCRCFailCount = 0;
	sendQeOccupied = false;
	virtualQueueSize = 0;
	localTXCount = 0;
	loopbackMsgPtr = &loopbackMsg;
	rimeaddr_copy(&notifyBurstyLinkNeighbor_m, &rimeaddr_node_addr);
	
	latestForwardedTableInit();

	process_start(&sendDataTask, NULL);	//start the sendDataTask

	process_start(&sendBeaconTask, NULL);	//start the sendBeaconTask
	clock_init();	//initialie clock for use in random_rand()
	random_init(clock_time());	//provide seed to RNG

	#ifndef BEACON_ONLY 
	process_start(&snoopHandlerTask, NULL);	//start the snoopHandlerTask
	#endif

	//Open connections
	broadcast_open(&broadcast, 129, &broadcast_call);
	unicast_open(&unicast, 130, &unicast_callbacks);

	//Register for callbacks
	register_cb_set_next_hop_address(routerForwarder_setNextHopAddress);
	register_cb_set_notify_bursty_neighbor(routerForwarder_setNotifyBurstyLinkNeighbor);

	return SUCCESS;
}
/*---------------------------------------------------------------------------*/
void chooseAdvertiseTime() {

	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static uint32_t rand =0;	

	//Create a 32bit pseudo random number
	rand = ((uint32_t)random_rand());
	rand = rand << 16;
	rand = rand | ((uint32_t)random_rand());

	t = currentInterval;
	t /= 2;
	t += rand % t;
	tHasPassed = false;
	isBeaconTimerPeriodic = false;
	ctimer_stop(&beacon_timer);

	#ifdef BEACON_ONLY
	timer_set(&beacon_timerTime, ULONG_MAX);
	#endif
	ctimer_set(&beacon_timer, t, beacon_timer_fired, 0);
  	timer_reset(&beacon_timerTime);  // Reset the tandem timer
}
/*---------------------------------------------------------------------------*/
void resetInterval() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	timer_set(&delayPacketTimer, ULONG_MAX);
	
	currentInterval = MININTERVAL;
	chooseAdvertiseTime();
}
/*---------------------------------------------------------------------------*/
void decayInterval() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	currentInterval *= 2;
	if (currentInterval > MAXINTERVAL) {
		currentInterval = MAXINTERVAL;
	}
	chooseAdvertiseTime();
}
/*---------------------------------------------------------------------------*/
void remainingInterval() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static clock_time_t remaining = 0;
 	remaining = currentInterval;
	remaining -= t;
	tHasPassed = true;
	isBeaconTimerPeriodic = false;
	ctimer_stop(&beacon_timer);
  // Stop the tandem timer here, but can't because API doesnt provide that functionality
	ctimer_set(&beacon_timer, remaining, beacon_timer_fired, 0);
  	timer_reset(&beacon_timerTime);  // Reset the tandem timer
}
/*---------------------------------------------------------------------------*/

uint32_t calcHdrChecksum(message_wrapper_t* msg) {

	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static uint32_t checksum;
	static bcp_data_header_t hdr;
  
  	hdr = msg -> bcp_data_header;
  	checksum = 0;

	//	This is a hop-by-hop checksum of the control header fields. 
	checksum += hdr.bcpDelay;
	checksum += hdr.bcpBackpressure;
	checksum += hdr.txCount;
	checksum += hdr.origin.u8[0] + hdr.origin.u8[1];
	checksum += hdr.hopCount;
	checksum += hdr.originSeqNo;
	checksum += hdr.pktType;
	return checksum;
}

void conditionalFQDiscard() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);

	static fe_queue_entry_t* discardQe;// = memb_alloc(&send_stack_mem); 

	if(list_length(send_stack) >= SEND_STACK_SIZE) {

		discardQe = list_chop(send_stack);
		list_remove(message_pool, discardQe -> msg);
		memb_free(&message_pool_mem, discardQe -> msg);

		list_remove(q_entry_pool, discardQe);
		memb_free(&q_entry_pool_mem, discardQe);

#ifdef VIRTQ 
		//	Dropped a data packet, increase virtual queue size
		virtualQueueSize++;
#endif
	}
}
/*---------------------------------------------------------------------------*/
void latestForwardedTableInit() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	latestForwardedTableActive = 0;
}
/*---------------------------------------------------------------------------*/
/* Returns the latestForwarded_table_entry pointer to the latestForwardedTable
*  entry representing the neighbor node address passed as a parameter. If
*  no such neighbor exists, returns NULL.
*/
latestForwarded_table_entry* latestForwardedTableFind(rimeaddr_t *neighbor_p) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static uint8_t i = 0;
	static bool isBroadcast = 0;
  
  isBroadcast = rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                                &rimeaddr_null);
	if (isBroadcast)
		return NULL;

	for (i = 0; i < latestForwardedTableActive; i++) {
		if (rimeaddr_cmp(&(latestForwardedTable[i].neighbor), neighbor_p))
		break;
	}

	if(i == latestForwardedTableActive)
		return NULL;

	return &(latestForwardedTable[i]);
}
/*---------------------------------------------------------------------------*/
error_t latestForwardedTableUpdate(rimeaddr_t *from_p, rimeaddr_t *origin_p, uint8_t originSeqNo_p, uint8_t hopCount_p) {

	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);

	static latestForwarded_table_entry *latestForwardedEntry;	
	latestForwardedEntry = latestForwardedTableFind(from_p);

	if (latestForwardedEntry == NULL && latestForwardedTableActive == ROUTING_TABLE_SIZE) {
		//	Not found and table is full
		pmesg(10, "ERROR in BcpForwardingEngine.c latestForwardedEntry: latestForwardedTable full, can't insert new neighbor.\n");
		return FAIL;
	}
	else if (latestForwardedEntry == NULL) {
		//	Not found and there is space
		//atomic {
			rimeaddr_copy(&(latestForwardedTable[latestForwardedTableActive].neighbor), from_p);
			rimeaddr_copy(&(latestForwardedTable[latestForwardedTableActive].origin), origin_p);           
			latestForwardedTable[latestForwardedTableActive].originSeqNo = originSeqNo_p;              
			latestForwardedTable[latestForwardedTableActive].hopCount = hopCount_p;
			latestForwardedTableActive++;
		//}
		pmesg(10, "New Entry: BcpForwardingEngine.c latestForwardedEntry\n");
	} 			
	else {
		//	Found, just update
		//atomic {
			rimeaddr_copy(&(latestForwardedEntry->origin), origin_p);
			latestForwardedEntry->originSeqNo = originSeqNo_p;
			latestForwardedEntry->hopCount = hopCount_p;
		//}
		pmesg(10, "Duplicate Entry: BcpForwardingEngine.c latestForwardedEntry\n");
	}
	return SUCCESS;
}  
/*---------------------------------------------------------------------------*/
/* Generate a beacon message, fill it with backpressure information
*  and broadcast it.  Other overhearing nodes will update their backpressure
*  information for this node.  There will bo no packet data in the broadcast.
*/
//THIS IS A TASK
//void sendBeaconTask() {

PROCESS_THREAD(sendBeaconTask, ev, data){
	PROCESS_BEGIN();
	while(1) {
		static uint32_t eval;

		PROCESS_WAIT_EVENT();  //wait here until an even occurs 
		pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);

		//	Store the local backpressure level to the backpressure field
		beaconHdr -> bcpBackpressure = list_length(send_stack) + sendQeOccupied + virtualQueueSize;
		beaconHdr -> type = beaconType;

		//	NO_SNOOP: low power sleep interval
	// #ifdef LOW_POWER_LISTENING
	// 	//	Call LowPowerListening.setRxSleepInterval(&beaconMsgBuffer, LPL_SLEEP_INTERVAL_MS);
	// 	call LowPowerListening.setRemoteWakeupInterval(&beaconMsgBuffer, LPL_SLEEP_INTERVAL_MS);
	// #endif


		rimeaddr_copy(&(beaconMsgBuffer.from), &rimeaddr_node_addr); 
		packetbuf_clear();
    	packetbuf_set_datalen(sizeof(bcp_beacon_header_t));
		packetbuf_copyfrom(&beaconMsgBuffer, sizeof(bcp_beacon_header_t));
	    eval = broadcast_send(&broadcast);

	    //Success
		if (eval != 0) {
			pmesg(200, "DEBUG: Beacon successfully called BeaconSend.send(). Eval = %d\n", eval);
		} 
		else {

			//	NO_SNOOP: add beacon type
			beaconType = NORMAL_BEACON;
			process_post(&sendBeaconTask, NULL, NULL);
		} 
	}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/*The snoopHandler does all routing processing after receiving a
* snoop message.  This allows the return statement in the snoop
* receive interface to immediately supply the radio with a new
* buffer, hopefully reducing the rate at which packets are corrupted
* or lost by the radio.*/

#ifndef BEACON_ONLY    
//void snoopHandlerTask() {
PROCESS_THREAD(snoopHandlerTask, ev, data) {
	PROCESS_BEGIN();
	while(1){
		PROCESS_WAIT_EVENT();
		pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
		static rimeaddr_t proximalSrc;
		// bcp_data_header_t *snoopPacket;
		// message_t* msg;

		static message_wrapper_t *snoopPacket; 

		if(list_length(snoop_queue) == 0)
			continue;//return;

		snoopPacket = list_pop(snoop_queue);

		rimeaddr_copy(&proximalSrc, &(snoopPacket -> from));

		//	I'm going to disable checksum checking of snooped packets, I think this is less critical
		//	Update the backlog information in the router
		routerForwarder_updateNeighborSnoop(rimeaddr_cmp(&(snoopPacket -> to), &rimeaddr_null),
													list_length(send_stack) + sendQeOccupied + virtualQueueSize,
													snoopPacket -> bcp_data_header.bcpBackpressure, 
													snoopPacket -> bcp_data_header.nhBackpressure, 
													snoopPacket -> bcp_data_header.nodeTxCount,
													&proximalSrc, 
													&(snoopPacket -> bcp_data_header.burstNotifyAddr));


	}

	PROCESS_END();
}
#endif
/*---------------------------------------------------------------------------*/
/*These is where all of the send logic is. When the ForwardingEngine
* wants to send a packet, it posts this task. The send logic is
* independent of whether it is a forwarded packet or a packet from
* a send client.
*
* The task first checks that there is a packet to send and that
* there is at least one neighbor. If the node is a collection
* root, it signals Receive with the loopback message. Otherwise,
* it sets the packet to be acknowledged and sends it. It does not
* remove the packet from the send queue: while sending, the 
* packet being sent is at the head of the queue; a packet is dequeued
* in the sendDone handler, either due to retransmission failure
* or to a successful send.
*/
//void sendDataTask() {
PROCESS_THREAD(sendDataTask, ev, data) {
	PROCESS_BEGIN();
	while(1) {
		PROCESS_WAIT_EVENT();
		pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);

		static fe_queue_entry_t* qe;
		static fe_queue_entry_t* nullQe;
		static message_wrapper_t* nullMsg;
		static bcp_data_header_t* nullHdr;
		static int subsendResult;
		static error_t retVal;
		static uint8_t payloadLen;
		static rimeaddr_t dest;
		static message_wrapper_t* hdr;
		static uint32_t sendTime;
		static uint32_t checksum;
		checksum = 0;

		//	Specialty handling of loopback or sudden sink designation
		if(rootControl_isRoot()) {
			sending = false; // If we are sending we'll abort

			if(sendQeOccupied == true) {
				qe = sendQe;
				sendQeOccupied = false; // Guaranteed succcessful service
			} 
			else {
				if(list_length(send_stack) == 0 && virtualQueueSize == 0) {
					//This shouldn't be possible
					pmesg(10, "FAILURE IN BCP_FORWARDING_ENGINE.c SENDDATATASK()");
					continue;
				}

				qe = sendQe = list_pop(send_stack);
			}
		
			memcpy(loopbackMsgPtr, qe -> msg, sizeof(message_wrapper_t));

			//Deallocate the message in qe
			list_remove(message_pool, qe -> msg); 
			memb_free(&message_pool_mem, qe -> msg);

			//Deallocate the qe object
			list_remove(q_entry_pool, qe);
			memb_free(&q_entry_pool_mem, qe);

			//Signal the event
			if(ev_msg_receive != NULL)
				loopbackMsgPtr = ev_msg_receive(loopbackMsgPtr);

			//Maybe do it again, if we are sink and there are data packets
			forwarderActivity();
			continue;	
		}

		if(sendQeOccupied == true) {
			qe = sendQe;
		}
		else {

			if(list_length(send_stack) == 0 && virtualQueueSize == 0) {
				pmesg(10, "ERROR: BcpForwardingEngine sendDataTask()\n");
				continue;
			}

			//Check to see whether there exists a neighbor to route to with positive weight.
			retVal = routerForwarder_updateRouting(list_length(send_stack) + sendQeOccupied + virtualQueueSize);

			//NO_SNOOP: add another retVal response type,
			//if there is no entry in our routing table
			//request a RR beacon
			if(retVal == ESIZE) {
				sending = false;
				pmesg(200, "DEBUG: RR Beacon Send\n");
				beaconType = RR_BEACON;
				process_post(&sendBeaconTask, NULL, NULL);

				//Stop the timer, reset it. We have two, one for keeping time,
				// one for the function call back
				ctimer_stop(&txRetryTimer);

				ctimer_set(&txRetryTimer, REROUTE_TIME, tx_retry_timer_fired, NULL);
				timer_reset(&txRetryTimerTime);
				
				continue;
			}

			if(retVal == FAIL) {
				//No neighbor is a good option right now, wait on a recompute-time
				sending = false;

				ctimer_stop(&txRetryTimer);

				ctimer_set(&txRetryTimer, REROUTE_TIME, tx_retry_timer_fired, NULL);
				timer_reset(&txRetryTimerTime);
				continue;
			}

			if(list_length(send_stack) == 0) {

				//	Create a null packet, place it on the stack (must be here by virtue of a virtual backlog)

				nullQe = memb_alloc(&q_entry_pool_mem);

				if(nullQe == NULL) {
					pmesg(10, "ERROR: BcpForwardingEngine - sendDataTask. Cannot enqueue nullQe\n");
					continue;
				}

				list_add(q_entry_pool, nullQe);

				nullMsg = memb_alloc(&message_pool_mem);
				if(nullMsg == NULL) {
					pmesg(10, "ERROR: BcpForwardingEngine - sendDataTask. Cannot enqueue nullMsg\n");

					//Deallocate
					list_remove(q_entry_pool, nullQe);
					memb_free(&q_entry_pool_mem, nullQe);

					continue;
				}

				list_add(message_pool, nullMsg);

				nullHdr = &(nullMsg -> bcp_data_header);
				nullHdr -> hopCount = 0;
				rimeaddr_copy(&(nullHdr -> origin), &rimeaddr_node_addr);
				nullHdr -> originSeqNo = nullSeqNo++;
				nullHdr -> bcpDelay = 0;
				nullHdr -> txCount = 0;
				nullHdr -> pktType = PKT_NULL;

				nullQe -> arrivalTime = 0; //call DelayPacketTimer.getNow(); 
				nullQe -> firstTxTime = 0;
				nullQe -> bcpArrivalDelay = 0;
				nullQe -> msg = nullMsg;
				nullQe -> source = LOCAL_SEND;
				nullQe -> txCount = 0; 

				list_push(send_stack, nullQe); 
	
				virtualQueueSize--;
			}

			qe = sendQe = list_pop(send_stack);

			pmesg(10, "SENDING MESSAGE ORIGINATING FROM = %d.%d\n", qe -> msg -> bcp_data_header.origin.u8[0],
				qe -> msg -> bcp_data_header.origin.u8[1]);

			qe -> firstTxTime = timer_remaining(&txRetryTimerTime);  //call txRetryTimer.getNow();
			sendQeOccupied = true;
		} //End else

		// payloadLen = sizeof(qe -> msg); //call SubPacket.payloadLength(qe->msg);

		//	Give up on a link after MAX_RETX_ATTEMPTS retransmit attempts, link is lousy!
		//	Furthermore, penalize by double MAX_RETX_ATTEMPTS, due to cutoff.
		if(qe -> txCount >= MAX_RETX_ATTEMPTS) {

			static bool isBroadcast = 0;
      		isBroadcast = rimeaddr_cmp(&(qe -> msg -> from), &rimeaddr_null);
      
			routerForwarder_updateLinkSuccess(&(qe -> msg -> from), isBroadcast, 2*MAX_RETX_ATTEMPTS); //call RouterForwarderIF.updateLinkSuccess(call AMDataPacket.destination(qe->msg), 2*MAX_RETX_ATTEMPTS);
			// call BcpDebugIF.reportValues( 0,0,0,0,0,MAX_RETX_ATTEMPTS, call AMDataPacket.destination(qe->msg),0x77 );
			
			qe -> txCount = 0;

			//	Place back on the Stack, discard element if necesary
			conditionalFQDiscard();

			list_push(send_stack, qe); // retVal = call SendStack.pushTop( qe );

			sendQeOccupied = false;

			//	Try again after a REROUTE_TIME, this choice was bad. 
			sending = false;

			ctimer_stop(&txRetryTimer);

			ctimer_set(&txRetryTimer, REROUTE_TIME, tx_retry_timer_fired, NULL);
			timer_reset(&txRetryTimerTime);

			continue;
		}

		qe -> txCount++;
		localTXCount++;
		rimeaddr_copy(&dest, &nextHopAddress_m);

		//Request an ack, not going to support DL without ack (for now)     

		//Store the local backpressure level to the backpressure field
		hdr = qe -> msg; //getHeader(qe->msg);
		hdr -> bcp_data_header.bcpBackpressure = list_length(send_stack) + sendQeOccupied + virtualQueueSize; 

		//Fill in the next hop Backpressure value
		hdr -> bcp_data_header.nhBackpressure = nextHopBackpressure_m;

		//Fill in the node tx count field (burst success detection by neighbors
#ifndef BEACON_ONLY
		
		hdr -> bcp_data_header.nodeTxCount = localTXCount;

		//	Fill in the burstNotifyAddr, then reset to TOS_NODE_ID immediately
		rimeaddr_copy(&(hdr->bcp_data_header.burstNotifyAddr), &notifyBurstyLinkNeighbor_m);
		rimeaddr_copy(&notifyBurstyLinkNeighbor_m, &rimeaddr_node_addr);
#endif

		//Update the txCount field
		hdr -> bcp_data_header.txCount = hdr -> bcp_data_header.txCount + 1;
		sendTime = 0; //This timer is never implemented in TinyOS: timer_remaining(&delayPacketTimer);

		//regardless of transmission history, lastTxTime and BcpDelay are re-comptued.
		hdr -> bcp_data_header.bcpDelay = qe -> bcpArrivalDelay + (sendTime - qe -> arrivalTime) + PER_HOP_MAC_DLY;

		//Calculate the checksum!
		checksum = calcHdrChecksum(qe -> msg);
		hdr -> bcp_data_header.hdrChecksum = checksum;

// #ifdef LOW_POWER_LISTENING 
// 		//	call LowPowerListening.setRxSleepInterval(qe->msg, LPL_SLEEP_INTERVAL_MS);
// 		call LowPowerListening.setRemoteWakeupInterval(qe->msg, LPL_SLEEP_INTERVAL_MS);
// #endif

		//Send thge packet!!
		rimeaddr_copy(&(qe -> msg -> to), &dest);
		rimeaddr_copy(&(qe -> msg -> from), &rimeaddr_node_addr);

		payloadLen = sizeof(message_wrapper_t); //call SubPacket.payloadLength(qe->msg);
		packetbuf_clear();
    	packetbuf_set_datalen(payloadLen);

		packetbuf_copyfrom(qe -> msg, payloadLen);

		pmesg(10, "Checksum from packet about to send: %u\n", ((message_wrapper_t*)packetbuf_dataptr()) -> bcp_data_header.hdrChecksum);

		//Non-zero if the packet could be sent, zero otherwise
		subsendResult = unicast_send(&unicast, &dest);

		//Success
		if(subsendResult != 0) {
			//	Successfully submitted to the data-link layer.
			pmesg(100, "BcpForwardingEngine: Successfully Sent Unicast Message\n");

			//Print out end-to-end message only if packet is originating from here
			if(rimeaddr_cmp(&(qe -> msg -> from), &(qe -> msg -> bcp_data_header.origin)) != 0)
				printf("Sent Packet from: %d.%d with SequenceNum = %lu\n", qe -> msg -> bcp_data_header.origin.u8[0], qe -> msg -> bcp_data_header.origin.u8[1],
					qe -> msg -> bcp_data_header.packetSeqNum);

			continue;
		}
		else {
			pmesg(100, "BcpForwardingEngine: Failed to Send Unicast Message. Trying again\n");
			// radioOn = false;

			//	NO_SNOOP: set beacon type
			beaconType = NORMAL_BEACON;
			process_post(&sendDataTask, NULL, NULL);
		}	

	} //End while(1)

	PROCESS_END();
} 
/*---------------------------------------------------------------------------*/
void forwarderActivity() {

	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);

	//When the forwarder not initialized, do nothing
	if(!isRunningForwardingEngine) {
		return;
	}
	else if(rootControl_isRoot()) {
		//As soon as we are root, should have zero virtual queue
		virtualQueueSize = 0;

		//The root should always be beaconing
		if(ctimer_expired(&beacon_timer)) {
			// 	If this is the start of the beacon, be agressive! Beacon fast
			//	so neighbors can re-direct rapidly.
			isBeaconTimerPeriodic = false;
			ctimer_set(&beacon_timer, FAST_BEACON_TIME, beacon_timer_fired, NULL);
     		timer_reset(&beacon_timerTime);  // Reset the tandem timer
		}

		//	If there are packets sitting in the forwarding queue, get them to the reciever
		if(list_length(send_stack) == 0) {
			//	 Don't need sending set to true here? Not using radio?
			//	NO_SNOOP: set beacon type
			beaconType = NORMAL_BEACON;

			process_post(&sendDataTask, NULL, NULL);
		}
		return;
	}
	else if (sending) {
		pmesg(100, "ForwardingEngine is already sending\n");
		return;
	}
	else if (!(list_length(send_stack) == 0) || virtualQueueSize > 0 || sendQeOccupied ) {
		//Stop Beacon, not needed if we are sending data packets (snoop)
#ifndef BEACON_ONLY
		ctimer_stop(&beacon_timer);
    // Stop the tandem timer here, but can't because API doesnt provide that functionality
#endif
		//	Start sending a data packet
		sending = true;

		//NO_SNOOP: set beacon type
		beaconType = NORMAL_BEACON;
		process_post(&sendDataTask, NULL, NULL);
		return;
	}
	else{
	//	Nothing to do but start the beacon!
#ifndef BEACON_ONLY
		isBeaconTimerPeriodic = true;
		ctimer_set(&beacon_timer, BEACON_TIME, beacon_timer_fired, NULL);
    	timer_reset(&beacon_timerTime);  // Reset the tandem timer
#endif
	}

	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
}
/*---------------------------------------------------------------------------*/
/*
* Function for preparing a packet for forwarding. Performs
* a buffer swap from the message pool. If there are no free
* message in the pool, it returns the passed message and does not
* put it on the send queue.
*/
message_wrapper_t* forward(message_wrapper_t* m, uint32_t arrivalTime_p) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static message_wrapper_t* newMsg;
	static fe_queue_entry_t *qe;
	static bcp_data_header_t *hdr;

	//	In the event of either LIFO type, if arrival finds a full LIFO
	//	discard the oldest element.
	conditionalFQDiscard();

	//Make sure list pool is not full
	if(list_length(message_pool) >= MESSAGE_POOL_SIZE) {
		pmesg(10, "WARNING: BcpForwardingEngine.c - forward. Cannot forward, message pool is out of memory.\n");
		return m;
	}
	//Make sure list pool is not full
	else if(list_length(q_entry_pool) >= Q_ENTRY_POOL_SIZE) {
		pmesg(10, "WARNING: BcpForwardingEngine.c - forward. Cannot forward, queue entry pool is out of memory.\n");
		return m;
	}

	qe = memb_alloc(&q_entry_pool_mem);
	if (qe == NULL) {
		pmesg(200, "WARNING: BcpForwardingEngine.c - forward. q_entry_pool is full.\n");
		return m;
	}
	list_add(q_entry_pool, qe);

	newMsg = memb_alloc(&message_pool_mem);
	if(newMsg == NULL) {
		pmesg(200, "WARNING: BcpForwardingEngine.c - forward. message_pool is full.\n");

		list_remove(q_entry_pool, qe);
		memb_free(&q_entry_pool_mem, qe);

		return m;
	}
	list_add(message_pool, newMsg);

	memset(newMsg, 0, sizeof(message_wrapper_t));	

    //  Copy the message, client may send more messages.
    memcpy(newMsg, m, sizeof(message_wrapper_t));

	hdr = &(newMsg -> bcp_data_header);

	qe -> msg = newMsg;
	qe -> source = FORWARD;
	qe -> arrivalTime = arrivalTime_p;
	qe -> txCount = 0;
	qe -> firstTxTime = 0;
	qe -> bcpArrivalDelay = hdr->bcpDelay;

	if(!(list_length(send_stack) >= SEND_STACK_SIZE)) {
#ifdef LIFO
		list_push(send_stack, qe);
#endif

#ifndef LIFO
		list_add(send_stack, qe);
#endif
		pmesg(200, "Forwarder is forwarding packet with send_stack size = %d\n", list_length(send_stack));
		forwarderActivity();

		// Successful function exit point:
		return newMsg;
	}

	else {
		//	There was a problem enqueuing to the send queue.
		//	Free the allocated MessagePool and QEntryPool
		list_remove(message_pool, newMsg);
		memb_free(&message_pool_mem, newMsg);

		list_remove(q_entry_pool, qe);
		memb_free(&q_entry_pool_mem, qe);
	}

	pmesg(10, "ERROR BcpForwardingEngine: Cannot forward, unable to allocate resources.\n");
	return m;
}
/*---------------------------------------------------------------------------*/
error_t bcpForwardingEngine_stdControlStart() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	isRunningForwardingEngine = true;

	//Start beaconing - we'll need this anyway =)
	//call BeaconTimer.startPeriodic(BEACON_TIME);
	resetInterval(); 

//NO_SNOOP: initial low power
// #ifdef LOW_POWER_LISTENING
// 	//	call LowPowerListening.setLocalSleepInterval(LPL_SLEEP_INTERVAL_MS); 
// 	call LowPowerListening.setLocalWakeupInterval(LPL_SLEEP_INTERVAL_MS);
// #endif

	return SUCCESS;    
}
/*---------------------------------------------------------------------------*/
error_t bcpForwardingEngine_stdControlStop() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	isRunningForwardingEngine = false;

	//Stop Beacons
	ctimer_stop(&beacon_timer);

	return SUCCESS;
}
/*---------------------------------------------------------------------------*/
error_t forwardingEngine_Send(message_wrapper_t* msg, uint8_t len) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	//	Send code for client send request    
	static bcp_data_header_t* hdr;
	static uint32_t arrivalTime; //uint32_t arrivalTime = call DelayPacketTimer.getNow(); 
	static error_t retVal;
	arrivalTime = 0;

	pmesg(200, "Forwarding Engine Sending Packet\n");
	if (!isRunningForwardingEngine) {return EOFF;}

	hdr = &(msg -> bcp_data_header);
	hdr -> hopCount = 0;
	rimeaddr_copy(&(hdr -> origin), &rimeaddr_node_addr);
	hdr -> originSeqNo  = seqno++;
	hdr -> bcpDelay = 0;
	hdr -> txCount = 0;
	hdr -> pktType = PKT_NORMAL;

	//	If needed, discard an element from the forwarding queue
	conditionalFQDiscard();

	//Make sure list pool is not full
	if(list_length(message_pool) >= MESSAGE_POOL_SIZE) {
		pmesg(200, "WARNING: BcpForwardingEngine.c - Send. Cannot send, message pool is out of memory.\n");
		return EBUSY;
	}
	//Make sure list pool is not full
	else if(list_length(q_entry_pool) >= Q_ENTRY_POOL_SIZE) {
		pmesg(200, "WARNING: BcpForwardingEngine.c - Send. Cannot send, queue entry pool is out of memory.\n");
		return EBUSY;
	}
	else {
		static message_wrapper_t* newMsg;
		static fe_queue_entry_t *qe;

		qe = memb_alloc(&q_entry_pool_mem);
		if (qe == NULL) {
			pmesg(10, "ERROR: BcpForwardingEngine.c - SEND. q_entry_pool is full.\n");
			return FAIL;
		}
		list_add(q_entry_pool, qe);

		newMsg = memb_alloc(&message_pool_mem);
		if (newMsg == NULL) {
			pmesg(10, "ERROR: BcpForwardingEngine.c - SEND. message_pool is full.\n");

			//	Free the QEntryPool
			list_remove(q_entry_pool, qe);
			memb_free(&q_entry_pool_mem, qe);
			return FAIL;
		}
		list_add(message_pool, newMsg);

		memset(newMsg, 0, sizeof(message_wrapper_t));

		//	Copy the message, client may send more messages.
		memcpy(newMsg, msg, sizeof(message_wrapper_t));

		qe -> msg = newMsg;
		qe -> source = LOCAL_SEND;
		qe -> arrivalTime = arrivalTime;
		qe -> txCount = 0;
		qe -> firstTxTime = 0;
		qe -> bcpArrivalDelay = 0;

		if(!(list_length(send_stack) >= SEND_STACK_SIZE)) {
#ifdef LIFO
			list_push(send_stack, qe);
#endif

#ifndef LIFO
			list_add(send_stack, qe);
#endif
			pmesg(100, "Forwarder is forwarding packet with send_stack size = %d\n", list_length(send_stack));
			forwarderActivity();

			if(ev_send_done != NULL)
				ev_send_done(msg, SUCCESS);
			// signal Send.sendDone(msg, SUCCESS);

			// Successful function exit point:
			return SUCCESS;
		}

		else {
			//	There was a problem enqueuing to the send queue.
			//	Free the allocated MessagePool and QEntryPool
			list_remove(message_pool, newMsg);
			memb_free(&message_pool_mem, newMsg);

			list_remove(q_entry_pool, qe);
			memb_free(&q_entry_pool_mem, qe);
		}
	}

	//	NB: at this point, we have a resource acquistion problem.
	//	Log the event, and drop the packet
	pmesg(10, "ERROR BcpForwardingEngine: Cannot SEND, unable to allocate resources.\n");
	return FAIL;
}
/*---------------------------------------------------------------------------*/
message_wrapper_t* subReceive_receive(message_wrapper_t* msg) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static rimeaddr_t from;
	static bcp_data_header_t * rcvPacket;
	static uint32_t checksum;
	static latestForwarded_table_entry *latestForwardedEntry;
	static uint32_t arrivalTime; //uint32_t arrivalTime = call DelayPacketTimer.getNow(); 

	checksum = 0;
	arrivalTime = 0;

	/*
	* A packet arrived destined to this AM_ADDR.  Handle bcpBackpressure update to
	*  the routing engine.  If we are the root, signal the receive event.  Otherwise
	*  we place the packet into the forwarding queue.
	*/ 
	//	Grab the backpressure value and send it to the router
	rimeaddr_copy(&from, &(msg -> from));
	rcvPacket = &(msg -> bcp_data_header);

	//	Calculate the checksum!
	checksum = calcHdrChecksum(msg);

	//Checksum and origin checks
	static uint32_t sentChecksum;
	static bcp_data_header_t hdr;

	hdr = msg -> bcp_data_header;
	sentChecksum = hdr.hdrChecksum;

	pmesg(200, "Calculated Checksum = %lu\n", checksum);
	pmesg(200, "Sent Checksum = %lu\n", sentChecksum);

	//	Verify checksum!
	if(checksum != sentChecksum) {
		//	Packet header failed checksum!
		//	I'm going to continue forwarding it, but disregard the control
		//	information.
		dataCRCFailCount++;

		//	NO_SNOOP: remove snoop
		pmesg(10, "BcpForwardingEngine Data CRC Failure\n");
		//	----------------------------------------------
		//	We cannot afford to forward corrupted messages, they can lead to bad behaviors
		return msg;
	} 
	else {
		dataCRCSuccessCount++; 

		//	Grab the latestForwardedEntry for this neighbor
		latestForwardedEntry = latestForwardedTableFind(&from);

		if(latestForwardedEntry == NULL) {
			//	Update the latestForwardedTable, this neighbor is unknown
			latestForwardedTableUpdate(&from, &(rcvPacket -> origin), rcvPacket -> originSeqNo, rcvPacket -> hopCount);
		}
		else if((rimeaddr_cmp(&(latestForwardedEntry -> origin), &(rcvPacket->origin)) != 0) &&
				latestForwardedEntry -> originSeqNo == rcvPacket -> originSeqNo &&
				latestForwardedEntry -> hopCount == rcvPacket -> hopCount) {

			/**
			* Duplicate suppresion
			*  We will store the last source / packetid / hop count receive per neighbor
			*  and reject any new arrival from that neighbor with identical parameters.
			*  This allows packets with identical source / packetid to be forwarded 
			*  through a node multiple times - necessary in a dynamic routing scenario.
			*/
			pmesg(10, "Duplicate packets\n");

			return msg;
		} 
		else {
			//	Update the latestForwardedTable
			latestForwardedTableUpdate(&from, &(rcvPacket->origin), rcvPacket->originSeqNo, rcvPacket->hopCount);
		}

		//	Update the backpressure information in the router
		bool isBroadcast = rimeaddr_cmp(&(msg -> to), &rimeaddr_null) != 0;
		routerForwarder_updateNeighborBackpressure(&from, isBroadcast, rcvPacket -> bcpBackpressure);
	}

	//	Retrieve the hopCount, increment it in the header
	rcvPacket->hopCount++;

	//	If I'm the root, signal receive. 
	if (rootControl_isRoot()) {

		if(ev_msg_receive != NULL)
			return ev_msg_receive(msg);
	}
	else {
		pmesg(100, "BcpForwardingEngine: Forwarding Packet\n");
		return forward(msg, arrivalTime);
	}    

	return msg;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*
* The second phase of a send operation; based on whether the transmission was
* successful, the ForwardingEngine either stops sending or starts the
* RetxmitTimer with an interval based on what has occured. If the send was
* successful or the maximum number of retransmissions has been reached, then
* the ForwardingEngine dequeues the current packet. If the packet is from a
* client it signals Send.sendDone(); if it is a forwarded packet it returns
* the packet and queue entry to their respective pools.
* 
*/  
void subSend_sendDone(message_wrapper_t* msg, int status) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static uint16_t txTimeMS;
	static uint32_t nowTime;
	static fe_queue_entry_t *qe;
	static bcp_data_header_t *sentPacket;
	txTimeMS = 0;
	nowTime = 0;
  
  	qe = sendQe;
	if (qe == NULL) {
		pmesg(10, "Error in ForwardingEngine: subSend_sendDone\n");
		return;
	}

	sentPacket = &(msg -> bcp_data_header);

	if (status != 0) {	
		//	CTP fears that "Immediate retransmission is the worst thing to do."
		//	I'm doing it anyway, simplicity is king for now.  I don't want to be
		//	having to justify all these constant parameter selections in a paper later.
		pmesg(10, "Failure in sending, retry\n");

		//	NO_SNOOP:Set beacon type
		beaconType = NORMAL_BEACON;
		//----------------------------------------------

		process_post(&sendDataTask, NULL, NULL);
	}
	else {
		//	A successfully sent packet
		pmesg(200, "BcpForwardingEngine - subSend_sendDone: Successfully forwarded a packet\n");

		//	Link rate estimation
		nowTime = 0;//nowTime = call DelayPacketTimer.getNow(); 

		if((nowTime - (qe -> firstTxTime)) > 0xFFFF)
			txTimeMS = 0xFFFF;
		else
			txTimeMS = (uint16_t)(nowTime - (qe -> firstTxTime));

		static bool isBroadcast = false;
		routerForwarder_updateLinkSuccess(&(msg -> to), isBroadcast, qe->txCount);
		routerForwarder_updateLinkRate(&(qe -> msg -> to), isBroadcast, txTimeMS);

		pmesg(10, "qe -> msg -> to = %d.%d.\n", qe -> msg -> to.u8[0], qe -> msg -> to.u8[1]);
		pmesg(10, "SendDataTask Dellocate messagepool size before: %d removing address: %p\n",
			list_length(message_pool),
			qe -> msg);

		list_remove(message_pool, qe -> msg);
		memb_free(&message_pool_mem, qe -> msg);

		pmesg(10, "SendDataTask Dellocate messagepool size after: %d removed address: %p\n",
						list_length(message_pool),
						qe -> msg);

		list_remove(q_entry_pool, qe);
		memb_free(&q_entry_pool_mem, qe);

		//	Only if successful do we set sending to FALSE;
		sendQeOccupied = false;
		sending = false;

		// call BcpDebugIF.reportBackpressure( call SendStack.size() + sendQeOccupied, call SendStack.size() + sendQeOccupied + virtualQueueSize, localTXCount, getHeader(msg)->origin, getHeader(msg)->originSeqNo, 1 );

		forwarderActivity(); 
	}
}
/*---------------------------------------------------------------------------*/
void beaconSend_sendDone(bcp_beacon_header_t* msg) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);

	// if(msg != &beaconMsgBuffer) 
	// 	pmesg(10, "Error in beaconsend.senddone. Something bad happened. look here please\n");
	  
	// else {
		pmesg(200, "BeaconSend done.\n");
		beaconSending = false;
		extraBeaconSending = false;
	// }
}
/*---------------------------------------------------------------------------*/
bcp_beacon_header_t* beaconReceive_receive(bcp_beacon_header_t* rcvBeacon) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static rimeaddr_t from;
	static bcp_beacon_header_t beacon;

	/* We need to error-check the beacon message, then parse it
	*  to inform the routing engine of new neighbor backlogs.
	*/
	// if (len != sizeof(bcp_beacon_header_t)) {
	// dbg("Beacon", "%s, received beacon of size %hhu, expected %i\n",__FUNCTION__, len,(int)sizeof(bcp_beacon_header_t));
	// return msg;
	// }

	beacon = *rcvBeacon;

	//	Grab the backpressure value and send it to the router
	rimeaddr_copy(&from, &(beacon.from));

	routerForwarder_updateNeighborBackpressure(&from, true, beacon.bcpBackpressure);

	if(beacon.type == RR_BEACON) {
		pmesg(200, "Receieved RR Beacon\n");
		beaconType = NORMAL_BEACON;
		process_post(&sendBeaconTask, NULL, NULL);
	}

	return rcvBeacon;
}
/*---------------------------------------------------------------------------*/
void beacon_timer_fired(){
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	#ifdef BEACON_ONLY    
	//	check stack size timer
 	if(ctimer_expired(&stack_check_timer)) {
		ctimer_set(&stack_check_timer, (clock_time_t)STACK_CHECK_INTERVAL, stack_check_timer_fired, NULL);
 	}
	#endif
	if (isRunningForwardingEngine) {
		if (!tHasPassed) {
			beaconType = NORMAL_BEACON;
			process_post(&sendBeaconTask, NULL, NULL);
 			remainingInterval();
		}
		else {
			decayInterval();
		}
 	}

	if(isBeaconTimerPeriodic == true){
		ctimer_set(&beacon_timer, BEACON_TIME, beacon_timer_fired, NULL);
    	timer_reset(&beacon_timerTime);  // Reset the tandem timer
  }
}
/*---------------------------------------------------------------------------*/
//	NO_SNOOP: stack size check timer
#ifdef BEACON_ONLY
void stack_check_timer_fired(void){
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static bool bpIncrease = false;
	static uint16_t diffBackpressure =0;
	static uint16_t newLocalBackpressure =0;
	static uint32_t beacon_time =0;

	beacon_time = ULONG_MAX - (uint32_t)timer_remaining(&beacon_timerTime);
	newLocalBackpressure = list_length(send_stack) + sendQeOccupied + virtualQueueSize;

	if(beacon_time >= BEACON_TH_INTERVAL){ //recently we have not broadcast a beacon
	//update backpressure to other nodes
	if(oldLocalBackpressure < newLocalBackpressure) 
	  bpIncrease = true;

	if(bpIncrease)
	  diffBackpressure = newLocalBackpressure - oldLocalBackpressure;
	else
	  diffBackpressure = oldLocalBackpressure - newLocalBackpressure;  


	if(diffBackpressure>=DIFF_QUEUE_TH){       	
	  if( extraBeaconSending == false){ 
	    extraBeaconSending = true;
	  } else { return; }
	  beaconType = NORMAL_BEACON;
	  process_post(&sendBeaconTask, NULL, NULL);
	  skipExtraBeaconCnt=3;
	}
	} else {
	  if(skipExtraBeaconCnt>0) {
	    skipExtraBeaconCnt--;
	  }
	}

	oldLocalBackpressure = newLocalBackpressure;
}
#endif
/*---------------------------------------------------------------------------*/
void tx_retry_timer_fired() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	if(sending == false) {
		sending = true;
		//	NO_SNOOP: set beacon type
		beaconType = NORMAL_BEACON;
		//	---------------------------

		process_post(&sendDataTask, NULL, NULL);
	}
}
/*---------------------------------------------------------------------------*/
#ifndef BEACON_ONLY     
message_wrapper_t* subSnoop_receive(message_wrapper_t* msg) {  
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static message_wrapper_t* newMsg;

	// if (len > call SubSend.maxPayloadLength()) {
	// 	dbg("ERROR","%s: snoop len > maxPayloadLength()!\n", __FUNCTION__);
	// 	call BcpDebugIF.reportError( 0x36 );
	// 	return msg;
	// }    

	/* In an effort to avoid radio-blocking of incomming messages, we are going to
	use a message queue for snoop messages (for now shared with forwarder) */
	if(list_length(snoop_queue) >= SNOOP_QUEUE_SIZE) {
		//No more room, SnoopQueue is full! Drop this message
		return msg;
	}
	else if (list_length(message_pool) >= MESSAGE_POOL_SIZE) {
		//No more pool space, have to drop this message
		return msg;
	}

	newMsg = memb_alloc(&message_pool_mem);
	if(newMsg == NULL) {
		pmesg(10, "ERROR: in subSnoop_receive\n");
		return msg;
	}
	list_add(message_pool, newMsg);


	list_add(snoop_queue, msg);
	// if( call SnoopQueue.enqueue( msg ) != SUCCESS ){
	// 	//Epic fail, dunno why
	// 	call MessagePool.put(newMsg);
	// 	return msg;
	// }

	process_post(&snoopHandlerTask, NULL, NULL);

	return newMsg;
}  

#endif
/*---------------------------------------------------------------------------*/
void routerForwarder_setNextHopAddress(rimeaddr_t *nextHopAddress_p, uint16_t nextHopBackpressure_p) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	rimeaddr_copy(&nextHopAddress_m, nextHopAddress_p);
	nextHopBackpressure_m = nextHopBackpressure_p;
}

void routerForwarder_setNotifyBurstyLinkNeighbor(rimeaddr_t *neighbor_p) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	rimeaddr_copy(&notifyBurstyLinkNeighbor_m, neighbor_p);
} 
/*---------------------------------------------------------------------------*/
// void delay_packet_timer_fired() {

// }
