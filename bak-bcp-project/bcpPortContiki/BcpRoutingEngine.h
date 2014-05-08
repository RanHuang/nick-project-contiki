#ifndef __BCPROUTINGENGINE_H__
#define __BCPROUTINGENGINE_H__
#include "Bcp.h"

/*********************Routing Engine Implementation***************************/
/*---------------------------------------------------------------------------*/
uint8_t 			routingTableActive;
routing_table_entry routingTable[ROUTING_TABLE_SIZE];
bool 				state_is_root;
bool 				isRunningRoutingEngine;
/*---------------------------------------------------------------------------*/
error_t 	bcpRoutingEngine_Init();
error_t 	bcpRoutingEngine_stdControlStart();
error_t 	bcpRoutingEngine_stdControlStop();
void 		bcpRoutingEngine_routingTableInit();
uint8_t 	bcpRoutingEngine_routingTableFind(rimeaddr_t *neighbor_p, bool isBroadcast);
error_t 	bcpRoutingEngine_routingTableUpdateEntry(rimeaddr_t *from_p, bool isBroadcast, uint16_t backpressure_p);
/*---------------------------------------------------------------------------*/

/********************Router Forwarder Implementation**************************/
/*---------------------------------------------------------------------------*/
//Begin Callbacks
typedef void (*event_set_next_hop_address)(rimeaddr_t *nextHopAddress_p, uint16_t nextHopBackpressure_p);
typedef void (*event_set_notify_bursty_neighbor)(rimeaddr_t *neighbor_p);

event_set_next_hop_address 			ev_next_hop;
event_set_notify_bursty_neighbor	ev_bursty_neighbor;

void 	register_cb_set_next_hop_address(event_set_next_hop_address function);
void 	register_cb_set_notify_bursty_neighbor(event_set_notify_bursty_neighbor function);
//End callbacks
/*---------------------------------------------------------------------------*/
error_t		routerForwarder_updateRouting(uint16_t localBackpressure_p);
void 		routerForwarder_txNoAck(rimeaddr_t *neighbor_p, bool isBroadcast);
void		routerForwarder_updateLinkSuccess(rimeaddr_t *neighbor_p, bool isBroadcast, uint8_t txCount_p);
void 		routerForwarder_updateLinkRate(rimeaddr_t *neighbor_p, bool isBroadcast, uint16_t newLinkPacketTxTime_p);
uint16_t	routerForwarder_getLinkRate(rimeaddr_t *neighbor_p, bool isBroadcast);
void 		routerForwarder_updateNeighborBackpressure(rimeaddr_t *neighbor_p, bool isBroadcast, uint16_t rcvBackpressure_p);
void 		routerForwarder_updateNeighborSnoop(	bool isBroadcast, uint16_t localBackpressure_p, uint16_t snoopBackpressure_p, 
													uint16_t nhBackpressure_p, uint8_t nodeTxCount_p,
													rimeaddr_t *neighbor_p, rimeaddr_t *burstNotifyAddr);
//Stub
void routerForwarder_txAck(rimeaddr_t *neighbor_p);
/*---------------------------------------------------------------------------*/

/**********************Root Control Implementation****************************/
error_t rootControl_setRoot();
error_t rootControl_unsetRoot();
bool 	rootControl_isRoot();
/*---------------------------------------------------------------------------*/

#endif /* __BCPROUTINGENGINE_H__ */