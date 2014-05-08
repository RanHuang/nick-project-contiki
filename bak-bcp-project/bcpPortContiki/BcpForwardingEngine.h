#ifndef __BCPFORWARDINGENGINE_H__
#define __BCPFORWARDINGENGINE_H__

#include "Bcp.h"

#define SEND_STACK_SIZE FORWARDING_QUEUE_SIZE - 1
#define SNOOP_QUEUE_SIZE 5

#define Q_ENTRY_POOL_SIZE FORWARDING_QUEUE_SIZE
#define MESSAGE_POOL_SIZE FORWARDING_QUEUE_SIZE + SNOOP_QUEUE_SIZE
#define SNOOP_POOL_SIZE SNOOP_QUEUE_SIZE

/* 	We supress duplicate packets (caussed by loss of 
 *		an ack message) using this routing-table-like 
 * 	structure.
 */
uint8_t latestForwardedTableActive;
latestForwarded_table_entry latestForwardedTable[ROUTING_TABLE_SIZE];

/* seqno stamps each outgoing bcp packet header with it's
 *  sequence number automatically.
 */
uint8_t seqno;
uint8_t nullSeqNo;

/* Used to transmit beacon messages during periods when the
 *  forwarder is turned off.
 */
bcp_beacon_header_t beaconMsgBuffer;	
bcp_beacon_header_t * beaconHdr;

/* beaconSending informs the beacon timer that we are already
 *  in the process of broadcasting a beacon.  Upon successful
 *  beaconSend.sendDone(), we set back to FALSE;
 */
bool beaconSending;

//	NO_SNOOP: add extra beacon when queue size dramatically changed

bool extraBeaconSending;
uint8_t skipExtraBeaconCnt;
uint16_t oldLocalBackpressure;

//	NO_SNOOP: initialize the beacon type

uint8_t beaconType;

/* Keeps track of whether the routing layer is running; if not,
 * it will not send packets. */
bool isRunningForwardingEngine;

/* Keeps track of whether the packet on the head of the queue
* is being used, and control access to the data-link layer.*/

bool sending;

/* Keeps track of whether the radio is on; no sense sending packets
 * if the radio is off. */
bool radioOn;

/* Used to track total transmission count by the local node */
uint16_t localTXCount;  

/* The routing engine stipulated next-hop address */
rimeaddr_t nextHopAddress_m;
uint16_t  nextHopBackpressure_m;

/* Initialized to TOS_NODE_ID, but set temporarily by the BcpRoutingEngine
 *  each time it recognizes a good link temporarily exists from a neighbor.
 */
rimeaddr_t notifyBurstyLinkNeighbor_m;

/* The loopback message is for when a collection roots calls
	 Send.send. Since Send passes a pointer but Receive allows
	 buffer swaps, the forwarder copies the sent packet into 
	 the loopbackMsgPtr and performs a buffer swap with it.
	 See sendTask(). */
message_wrapper_t loopbackMsg;	//message_t loopbackMsg; 
message_wrapper_t *loopbackMsgPtr;	//message_t* loopbackMsgPtr;		// ? ONE_NOK was removed; unsure what it is

/* The sendQe is used to store the current packet being
 *  attempted at transmission.  This is necessary when
 *  stacks are being used - as subsequent arrivals
 *  may cause the stack to change - bad mojo!
 */
fe_queue_entry_t* sendQe;
bool sendQeOccupied;

/* The virtual queue preserves backpressure values through
 *  stack drop events.  This preserves performance of
 *  BCP.  If a forwarding event occurs while the data stack
 *  is empty, a null packet is generated from this virtual
 *  queue backlog.
 */
uint16_t virtualQueueSize;

/* These counters track failed and successful CRC counts
	 for debugging purposes. */
uint16_t dataCRCSuccessCount;
uint16_t dataCRCFailCount;
uint16_t snoopCRCSuccessCount;
uint16_t snoopCRCFailCount;

//	NO_SNOOP: adaptive beacon

uint32_t currentInterval;
clock_time_t t; 
bool tHasPassed;
/*---------------------------------------------------------------------------*/
bool isBeaconTimerPeriodic;
struct ctimer beacon_timer;
struct timer beacon_timerTime; // Kluge train fix to provide '.getNow'
struct ctimer stack_check_timer;
struct ctimer txRetryTimer;
struct timer	 txRetryTimerTime;	//Choo choo, all aboard the kluge train. We need to have a "timer.getNow"
										//Sadly, only provided by timer (not ctimer). Let's run them in tandem!
struct timer  delayPacketTimer;
/*---------------------------------------------------------------------------*/
//Send Stack
MEMB(send_stack_mem, fe_queue_entry_t, SEND_STACK_SIZE);
LIST(send_stack); 
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
//Snoop Queue
MEMB(snoop_queue_mem, message_wrapper_t, SNOOP_QUEUE_SIZE);
LIST(snoop_queue); 
/*---------------------------------------------------------------------------*/
//Pool - QENTRYPOOL
MEMB(q_entry_pool_mem, fe_queue_entry_t, Q_ENTRY_POOL_SIZE);
LIST(q_entry_pool); 

//Pool - MESSAGEPOOL
MEMB(message_pool_mem, message_wrapper_t, MESSAGE_POOL_SIZE);
LIST(message_pool); 

//Pool - SNOOPPOOL
MEMB(snoop_pool_mem, message_wrapper_t, SNOOP_POOL_SIZE);
LIST(snoop_pool); 
/*---------------------------------------------------------------------------*/
//Begin Callbacks
typedef message_wrapper_t* (*event_message_receive)(message_wrapper_t *msg);
typedef void (*event_send_done)(message_wrapper_t *msg, error_t err);

event_message_receive ev_msg_receive;
event_send_done ev_send_done;

void register_cb_message_receive(event_message_receive function);
void register_cb_send_done(event_send_done function);
/*---------------------------------------------------------------------------*/
//Function prototypes
error_t bcp_forwarding_engine_init();
void chooseAdvertiseTime();
uint32_t calcHdrChecksum(message_wrapper_t* msg);
void resetInterval();
void decayInterval();
void remainingInterval();
void conditionalFQDiscard();
void latestForwardedTableInit();
latestForwarded_table_entry* latestForwardedTableFind(rimeaddr_t *neighbor_p);

void beacon_timer_fired();
void stack_check_timer_fired();
void tx_retry_timer_fired();
error_t bcpForwardingEngine_stdControlStart();
error_t bcpForwardingEngine_stdControlStop();
error_t forwardingEngine_Send(message_wrapper_t* msg, uint8_t len);
// void delay_packet_timer_fired();

void forwarderActivity();
message_wrapper_t* forward(message_wrapper_t* m, uint32_t arrivalTime_p);
message_wrapper_t* subReceive_receive(message_wrapper_t* msg);
void subSend_sendDone(message_wrapper_t* msg, int status);
void routerForwarder_setNextHopAddress(rimeaddr_t *nextHopAddress_p, uint16_t nextHopBackpressure_p);
void routerForwarder_setNotifyBurstyLinkNeighbor(rimeaddr_t *neighbor_p);
bcp_beacon_header_t* beaconReceive_receive(bcp_beacon_header_t* rcvBeacon);
void beaconSend_sendDone(bcp_beacon_header_t* msg);
message_wrapper_t* subSnoop_receive(message_wrapper_t* msg);
/*---------------------------------------------------------------------------*/

#endif /* __BCPFORWARDINGENGINE_H__ */