#include "BcpRoutingEngine.h"

/*********************Routing Engine Implementation***************************/
/*---------------------------------------------------------------------------*/
error_t bcpRoutingEngine_Init() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	isRunningRoutingEngine = false;
	bcpRoutingEngine_routingTableInit();

	//Return success
	return SUCCESS;
}
/*---------------------------------------------------------------------------*/
error_t bcpRoutingEngine_stdControlStart() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	if(!isRunningRoutingEngine) {
		isRunningRoutingEngine = true;
	}

	//Return success
	return SUCCESS;
}
/*---------------------------------------------------------------------------*/
error_t bcpRoutingEngine_stdControlStop() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	isRunningRoutingEngine = false;

	//Return success
	return SUCCESS;
}
/*---------------------------------------------------------------------------*/
void bcpRoutingEngine_routingTableInit() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	routingTableActive = 0;
}
/*---------------------------------------------------------------------------*/
uint8_t bcpRoutingEngine_routingTableFind(rimeaddr_t *neighbor_p, bool isBroadcast) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static uint8_t i;
	if(isBroadcast)
		i = routingTableActive;
	else {
		for(i = 0; i < routingTableActive; i++) {
			if(rimeaddr_cmp(&(routingTable[i].neighbor), neighbor_p) != 0)
				break;
		}
	}

	return i;
}
/*---------------------------------------------------------------------------*/
error_t bcpRoutingEngine_routingTableUpdateEntry(rimeaddr_t *from_p, bool isBroadcast, uint16_t backpressure_p) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static uint8_t idx;

	idx = bcpRoutingEngine_routingTableFind(from_p, isBroadcast);
	if(idx == ROUTING_TABLE_SIZE) {
		//Not found and table is full. No replacement

		//FAIL
		return FAIL;
	}
	else if(idx == routingTableActive) {
		//Not found and space is available
		
		routingTable[idx].neighbor = *from_p;
		routingTable[idx].backpressure = backpressure_p;
		routingTable[idx].linkPacketTxTime = 1;	//Default to 1 MS
		routingTable[idx].linkETX = 100;			//Initialize to lossless link

		#ifndef BEACON_ONLY
		routingTable[idx].lastTxNoStreakID = 0;	
		routingTable[idx].txNoStreakCount = 0;	//No burst successes
		routingTable[idx].isBurstyNow = false;	//Not bursty presently
		#endif

		routingTableActive++;

		pmesg(100, "Routing OK, new entry: idx %d, neighbor: %d.%d, backpressure %d.\n",
					 routingTableActive-1, 
					 from_p -> u8[0],
					 from_p -> u8[1],
					 backpressure_p);
	}
	else {
		//Entry found, just update it
		routingTable[idx].backpressure = backpressure_p;

		pmesg(100, "Routing OK, entry found, updating: idx %d, neighbor: %d.%d, backpressure %d.\n",
					 routingTableActive-1, 
					 from_p -> u8[0],
					 from_p -> u8[1],
					 backpressure_p);
	}

	return SUCCESS;
}
/*---------------------------------------------------------------------------*/
/********************Router Forwarder Implementation**************************/

//Register for the Set Next Hop Address event
void register_cb_set_next_hop_address(event_set_next_hop_address function) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	ev_next_hop = function;
}
//Register for the Set Notify Bursty Neighbor event
void register_cb_set_notify_bursty_neighbor(event_set_notify_bursty_neighbor function) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	ev_bursty_neighbor = function;
}
/*---------------------------------------------------------------------------*/
//Stub
void routerForwarder_txAck(rimeaddr_t *neighbor_p){ }
/*---------------------------------------------------------------------------*/
//Function to update routing table
error_t routerForwarder_updateRouting(uint16_t localBackpressure_p) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static uint8_t compareIdx;
	static int32_t compareWeight;
	static uint8_t maxWeightIdx;
	static int32_t maxWeight;
	static rimeaddr_t bestNeighbor;
	compareIdx = 0;
	compareWeight = 0;
	maxWeightIdx = 0;
	maxWeight = -1;
	rimeaddr_copy(&bestNeighbor, &rimeaddr_null);
	#ifndef BEACON_ONLY
	static bool     burstNeighborFound;
	static uint8_t  bestBurstNeighborIdx;
	static uint16_t bestBurstNeighborBackpressure;
	burstNeighborFound = 0;
	bestBurstNeighborIdx = 0;
	bestBurstNeighborBackpressure = 0;
	#endif 

	//Empty table, fail
	if(routingTableActive == 0)
		return ESIZE;

	//Not found and space is available
	static uint32_t longLocalBackpressure;
	static uint32_t longNeighborBackpressure;
	static uint32_t longLinkETX;
	static uint32_t longLinkRate;

	longLocalBackpressure = 0;
	longNeighborBackpressure = 0;
	longLinkETX = 0;
	longLinkRate = 0;
  
  longLocalBackpressure = localBackpressure_p;
  

	for (compareIdx = 0; compareIdx < routingTableActive; compareIdx++) {
		// Scan through the routingTable and find the best neighbor
		//  This is the neighbor with the largest weight value, unless
		//  there exist neighbors with isBurstyNow set, in which case
		//  we send to the bursty neighbor so long as its backpressure value
		//  is strictly less than or equal to the best weight option.
		#ifndef BEACON_ONLY
		if(routingTable[compareIdx].isBurstyNow == true) {
			// Only evaluate an isBurstyNow link once.  If it is selected, then
			//  we will re-set the isBurstyNow bit which is cleared upon first failed
			//  data ack.
			routingTable[compareIdx].isBurstyNow = false;
			if(!burstNeighborFound || bestBurstNeighborBackpressure > routingTable[compareIdx].backpressure ) { 
				burstNeighborFound = 1;
				bestBurstNeighborIdx = compareIdx;
				bestBurstNeighborBackpressure = routingTable[compareIdx].backpressure;
			}
		}
		else {
		#endif
			// Convert link transmit time to packets / minute 
			longNeighborBackpressure = routingTable[compareIdx].backpressure;
			longLinkETX = LINK_LOSS_V * routingTable[compareIdx].linkETX / 100;
			longLinkRate = 10000 / routingTable[compareIdx].linkPacketTxTime; // Packets per minute
			compareWeight = (longLocalBackpressure - longNeighborBackpressure - longLinkETX) * longLinkRate;

			if (compareWeight > maxWeight ) {
				maxWeightIdx = compareIdx;
				maxWeight = compareWeight; 
			}
		#ifndef BEACON_ONLY
		}
		#endif
	}

	#ifndef BEACON_ONLY
	if(burstNeighborFound && routingTable[maxWeightIdx].backpressure >= bestBurstNeighborBackpressure) {
		// We'll use this burst neighbor, reset the isBurstyNow which we cleared for all bursty links
		maxWeight = (int32_t)localBackpressure_p - (int32_t)bestBurstNeighborBackpressure - LINK_LOSS_V;

		if(maxWeight > 0) {
			// Only preserve isBurstyNow if we are going to route to it during this decisions point.
			//  But what if we are simply stalled, waiting for any neighbor to have proper backlog values?
			// call BcpDebugIF.reportValues( localBackpressure_p,bestBurstNeighborBackpressure,0,0,0,0,bestNeighbor,0x12 );
			routingTable[bestBurstNeighborIdx].isBurstyNow = true;
		}

		rimeaddr_copy(&bestNeighbor, &(routingTable[bestBurstNeighborIdx].neighbor));
		maxWeightIdx = bestBurstNeighborIdx;
	} 
	else {
		rimeaddr_copy(&bestNeighbor, &(routingTable[maxWeightIdx].neighbor));
	}
	#else
	rimeaddr_copy(&bestNeighbor, &(routingTable[maxWeightIdx].neighbor));
	#endif 

	if(maxWeight <= 0)
		// There exists no neighbor that we want to transmit to at this time,
		//  notify the forwarding engine!
		return FAIL;
	 
	pmesg(100, "%s :: %s :: Best Neighbor = %d.%d\n", __FILE__, __func__, bestNeighbor.u8[0], bestNeighbor.u8[1]);

	// There is a neighbor we should be transmitting to. Signal event
	ev_next_hop(&bestNeighbor, routingTable[maxWeightIdx].backpressure);

	return SUCCESS;
}
/*---------------------------------------------------------------------------*/
void routerForwarder_txNoAck(rimeaddr_t *neighbor_p, bool isBroadcast) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static uint8_t routingTableIdx; 

	// If operating on a bursty link, abort
	routingTableIdx = bcpRoutingEngine_routingTableFind(neighbor_p, isBroadcast);
	if (routingTableIdx == ROUTING_TABLE_SIZE) {
		// Trouble, couldn't find neighbor
		pmesg(10, "ERROR: BcpRoutingEngine.c: routerForwarder_txNoAck\n");
		return;
	}

	#ifndef BEACON_ONLY
	// If this link was bursty, clear it
	if(routingTable[routingTableIdx].isBurstyNow == true) 
		routingTable[routingTableIdx].isBurstyNow = false;
	#endif
}
/*---------------------------------------------------------------------------*/
void routerForwarder_updateLinkSuccess(rimeaddr_t *neighbor_p, bool isBroadcast, uint8_t txCount_p) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static uint32_t newETX;
	static uint16_t oldETX;
	static uint8_t idx;

	idx = bcpRoutingEngine_routingTableFind(neighbor_p, isBroadcast);

	if( idx == routingTableActive ) {
		// Could not find this neighbor, error!
		pmesg(10, "ERROR: BcpRoutingEngine.c: routerForwarder_updateLinkSuccess\n");
		return;
	}

	oldETX = routingTable[idx].linkETX;

	// Found neighbor, update link loss rate
	newETX = routingTable[idx].linkETX;
	newETX = (newETX * LINK_LOSS_ALPHA + txCount_p * 100 * (100 - LINK_LOSS_ALPHA ) ) / 100;
	routingTable[idx].linkETX = (uint16_t)newETX;
}
/*---------------------------------------------------------------------------*/
void routerForwarder_updateLinkRate(rimeaddr_t *neighbor_p, bool isBroadcast, uint16_t newLinkPacketTxTime_p) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static uint16_t previousLinkPacketTxTime;
	static uint16_t newLinkPacketTxTime;
	static uint8_t routingTableIdx;
	routingTableIdx = 0;
  
  routingTableIdx = bcpRoutingEngine_routingTableFind(neighbor_p, isBroadcast);

	// Do we need to recompute routes after a updateLinkRate call?
	//  I think perhaps we should (though this may not be absolutely necessary).
	//  Does incur an overhead.
	if(routingTableIdx == routingTableActive ) {
		// Neighbor not found
		pmesg(10, "ERROR: BcpRoutingEngine.c: routerForwarder_updateLinkRate\n");
		return;
	}

	// Floor the delay value to 1 MS! Should be impossibly fast, but just in case.
	// Otherwise backpressure will be ignored for fast links.
	if(newLinkPacketTxTime_p == 0)
		newLinkPacketTxTime = 1;
	else
		newLinkPacketTxTime = newLinkPacketTxTime_p;

	//ATOMIC (
	previousLinkPacketTxTime = routingTable[routingTableIdx].linkPacketTxTime;

	// Update the estimated packet transmission time for the link.
	// Use exponential weighted avg.
	routingTable[routingTableIdx].linkPacketTxTime =
		((LINK_EST_ALPHA * (uint32_t)(routingTable[routingTableIdx].linkPacketTxTime)) +
		(10 - LINK_EST_ALPHA)*(uint32_t)newLinkPacketTxTime) / 10;
	//)END ATOMIC

	// call BcpDebugIF.reportLinkRate(neighbor_p,previousLinkPacketTxTime, newLinkPacketTxTime, 
	//              routingTable[routingTableIdx].linkPacketTxTime, routingTable[routingTableIdx].linkETX);
}
/*---------------------------------------------------------------------------*/
uint16_t routerForwarder_getLinkRate(rimeaddr_t *neighbor_p, bool isBroadcast) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static uint8_t routingTableIdx;
	routingTableIdx = 0;
  
  routingTableIdx = bcpRoutingEngine_routingTableFind(neighbor_p, isBroadcast);

	if(routingTableIdx == routingTableActive) {
		// Neighbor not found
		pmesg(10, "ERROR: BcpRoutingEngine.c: routerForwarder_getLinkRate\n");
		return 0xFFFF;
	}

	return routingTable[routingTableIdx].linkPacketTxTime;
}
/*---------------------------------------------------------------------------*/
void routerForwarder_updateNeighborBackpressure(rimeaddr_t *neighbor_p, bool isBroadcast, uint16_t rcvBackpressure_p) {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	/**
	* Update the backpressure associated with the overheard neighbor.
	*/
	bcpRoutingEngine_routingTableUpdateEntry(neighbor_p, isBroadcast, rcvBackpressure_p);
}
#ifndef BEACON_ONLY
/*---------------------------------------------------------------------------*/
void routerForwarder_updateNeighborSnoop(	bool isBroadcast, uint16_t localBackpressure_p, uint16_t snoopBackpressure_p, 
											uint16_t nhBackpressure_p, uint8_t nodeTxCount_p,
											rimeaddr_t *neighbor_p, rimeaddr_t *burstNotifyAddr) {

	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	static uint8_t idx;

	/**
	* Update the backpressure associated with the overheard neighbor.
	*/
	bcpRoutingEngine_routingTableUpdateEntry(neighbor_p, isBroadcast, snoopBackpressure_p);

	/**
	* Check for burst of successes on the link
	*/
	idx = bcpRoutingEngine_routingTableFind(neighbor_p, isBroadcast);

	if(routingTable[idx].lastTxNoStreakID + 1 == nodeTxCount_p) {
		// The burst of successes continues! Kudos! Increment stuff.
		routingTable[idx].lastTxNoStreakID++;
		routingTable[idx].txNoStreakCount++;

		if(routingTable[idx].txNoStreakCount == 3 && localBackpressure_p < nhBackpressure_p) {
			// 3 successful sequential transmissions indicates that this is a good link and we
			//  checked that the local backpressure is less than that stored in the snooped packet
			// It's very likely that we are a good link option now, so attempt to notify this neighbor
			// call BcpDebugIF.reportValues( 0,0,0,0,0,0,neighbor_p,0xA1 );
			// signal RouterForwarderIF.setNotifyBurstyLinkNeighbor(neighbor_p);

			//Signal Event
			ev_bursty_neighbor(neighbor_p);
		}
	} 
	else {
		//Lost a transmission. Reset streak
		//call BcpDebugIF.reportValues( 0,0,0,0,0,0,neighbor_p,0xA2 );
		routingTable[idx].lastTxNoStreakID = nodeTxCount_p;
		routingTable[idx].txNoStreakCount = 0;
	}

	/**
	* Check to see if this neighbor is telling us we have bursty successes to them.
	*/
	if(rimeaddr_cmp(burstNotifyAddr, &rimeaddr_node_addr)) {
		// Set the isBurstyNow bit for this neighbor
		// call BcpDebugIF.reportValues( 0,0,0,0,0,0,neighbor_p,0xA3 );
		routingTable[idx].isBurstyNow = true;
	}
}
/*---------------------------------------------------------------------------*/
#endif  
/**********************Root Control Implementation****************************/
/* RootControl interface */
/** sets the current node as a root, if not already a root */
error_t rootControl_setRoot() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	state_is_root = true;

	pmesg(10, "I AM ROOT NOW\n");
	return SUCCESS;
}
/*---------------------------------------------------------------------------*/
error_t rootControl_unsetRoot() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	state_is_root = false;

	pmesg(10, "I AM NO LONGER ROOT\n");
	return SUCCESS;
}
/*---------------------------------------------------------------------------*/
bool rootControl_isRoot() {
	pmesg(200, "%s :: %s :: Line #%d\n", __FILE__, __func__, __LINE__);
	return state_is_root;
}