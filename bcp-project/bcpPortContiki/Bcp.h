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
#ifndef __BCP_H__
#define __BCP_H__

#include "contiki.h"
#include "net/rime.h"
#include <stdio.h>
#include <stdbool.h>
#include "sys/ctimer.h"
#include "sys/timer.h"
#include "lib/random.h"
#include "sys/clock.h"
#include "mydebug.h"

#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__);
#else
#define PRINTF(...)
#endif

/* The times below are being converted into platform specific clock timings */
//128 milliseconds
#define MININTERVAL (((uint32_t)(CLOCK_SECOND * 0.128f)) == (0) ? (1) : ((uint32_t)(CLOCK_SECOND * 0.128f)))

//512 seconds
#define MAXINTERVAL (((uint32_t)(CLOCK_SECOND * 512)) == (0) ? (1) : ((uint32_t)(CLOCK_SECOND * 512)))

//8 seconds
#define BEACON_INTERVAL (((uint32_t)(CLOCK_SECOND * 8.192f)) == (0) ? (1) : ((uint32_t)(CLOCK_SECOND * 8.192f)))

#define BEACON_ONLY 1
#define LIFO 1

/**
 * STACK_CHECK_INTERVAL is for extra beacon to check the change of queueze
 * BEACON_TH_INTERVAL: if the time between last beacon and current check
 * interval is less than this threshold, we don't send a beacon.
 * If it is large than the threshold, we send an extra beacon
 */
#define STACK_CHECK_INTERVAL (((uint32_t)(BEACON_INTERVAL / 10)) == (0) ? (1) : ((uint32_t)(BEACON_INTERVAL / 10)))
#define BEACON_TH_INTERVAL STACK_CHECK_INTERVAL

/**
 * PKT_NORMAL: Indicates that a BCP packet contains source-originated data.
 * PKT_NULL: Indicates that a BCP packet was dropped, and a subsequent virtual
 *           packet was eventually forwarded.
 */
enum{
  PKT_NORMAL = 1,
  PKT_NULL   = 2
};

/*
 * The number of times the ForwardingEngine will try to
 * transmit a packet before giving up if the link layer
 * supports acknowledgments. If the link layer does
 * not support acknowledgments it sends the packet once.
 */
 /* The times below are being converted into platform specific clock timings */
//5 seconds
#define BEACON_TIME CLOCK_SECOND * 5
//25 milliseconds
#define FAST_BEACON_TIME (((uint32_t)(CLOCK_SECOND * 0.035f)) == (0) ? (1) : ((uint32_t)(CLOCK_SECOND * 0.035f)))
//1 second
#define LOG_UPDATE_TIME CLOCK_SECOND
//50 milliseconds
#define REROUTE_TIME (((uint32_t)(CLOCK_SECOND * 0.05f)) == (0) ? (1) : ((uint32_t)(CLOCK_SECOND * 0.05f)))
//40 milliseconds
#define MAX_FWD_DLY (((uint32_t)(CLOCK_SECOND * 0.04f)) == (0) ? (1) : ((uint32_t)(CLOCK_SECOND * 0.04f)))
enum {
  // BEACON_TIME        = 5000,  // Milliseconds
  // FAST_BEACON_TIME   = 35,    // Milliseconds
  // LOG_UPDATE_TIME    = 1000,  // Milliseconds
  // REROUTE_TIME       = 50,    // Milliseconds
  MAX_RETX_ATTEMPTS  = 5,     // Maximum retransmit count per link
  ROUTING_TABLE_SIZE = 0x30,  // Max Neighbor Count
  FORWARDING_QUEUE_SIZE = 25, // Maximum forwarding queue size
  DIFF_QUEUE_TH      = 4,     // when queue size differential is >= DIFF_QUEUE_TH, send extra beacon
  SNOOP_QUEUE_SIZE   = 0x5,   // Maximum snoop queue size
  // MAX_FWD_DLY        = 40,    // Milliseconds of delay to forward per delay packet
  FWD_DLY_PKT_COUNT  = 20,    // Number of estimated packet xmission times to wait between delayPackets
  LINK_EST_ALPHA     = 9,     // Decay parameter. 9 = 90% weight of previous rate Estimation
  LINK_LOSS_ALPHA    = 90,    // Decay parameter. 90 = 90% weight of previous link loss Estimate
  LINK_LOSS_V        = 2,     // V Value used to weight link losses in Lyapunov Calculation
  PER_HOP_MAC_DLY    = 10     // Typical per-hop MAC delay of successful transmission on Tmote Sky 
};

enum {
    // AM types:
    AM_BCP_BEACON  = 0x90,
    AM_BCP_DATA    = 0x91,
    AM_BCP_DELAY   = 0x92
};

/*
 * The network header that the ForwardingEngine introduces.
 */
typedef struct {
  uint32_t      packetSeqNum;
  uint32_t      bcpDelay;         // Delay experienced by this packet
  uint16_t      bcpBackpressure;  // Node backpressure measurement for neighbors
  uint16_t      nhBackpressure;   // Next hop Backpressure, used by STLE, overheard by neighbors  
  uint16_t      txCount;          // Total transmission count experienced by the packet
  uint32_t      hdrChecksum;      // Checksum over origin, hopCount, and originSeqNo
  rimeaddr_t    origin;
  uint8_t       hopCount;         // End-to-end hop count experienced by the packet
  uint8_t       originSeqNo;         
  uint8_t       pktType;          // PKT_NORMAL | PKT_NULL
  #ifndef BEACON_ONLY
  uint8_t       nodeTxCount;      // Increment every tx by this node, to determine bursts for STLE
  rimeaddr_t    burstNotifyAddr;  // In the event of a burst link available detect, set neighbor addr, else set self addr
  #endif 
} bcp_data_header_t;

/*
 * This is wrapper for the BCP Data Header type so we store
 * data like to and from addresses
 */
typedef struct {
  struct message_wrapper_t *next;
  rimeaddr_t to;
  rimeaddr_t from;
  bcp_data_header_t bcp_data_header;
} message_wrapper_t;

 /*
 * NORMAL_BEACON: for general anounce backpressure
 * RR_BEACON: is a request route beacon, 
 * for late joiner to discover the neighbor quickly
 */
enum {
  NORMAL_BEACON = 1,
  RR_BEACON = 2
};

/*
 * The network header that the Beacons use.
 */
typedef struct {
  rimeaddr_t from;
  uint16_t  bcpBackpressure;
  uint8_t   type;         //to let receiver know if I received a request route beacon
} bcp_beacon_header_t;

/*
 * The network header that delay packets use.
 */
typedef struct {
  uint32_t  bcpTransferDelay;
  uint32_t  bcpBackpressure; 
  uint8_t   delaySeqNo;
  uint8_t   avgHopCount;      // Exponental moving average of hop count seen by this node
} bcp_delay_header_t;

/*
 * Defines used to determine the source of packets within
 *  the forwarding queue.  
 */
enum {
    LOCAL_SEND = 0x1,
    FORWARD    = 0x2,
};

/*
 * An element in the ForwardingEngine send queue.
 * The client field keeps track of which send client 
 * submitted the packet or if the packet is being forwarded
 * from another node (client == 255). Retries keeps track
 * of how many times the packet has been transmitted.
 */
typedef struct {
  struct fe_queue_entry_t *next;
  uint32_t bcpArrivalDelay;
  uint32_t arrivalTime;
  uint32_t firstTxTime;
  uint32_t lastTxTime;
  uint8_t  source;
  message_wrapper_t *msg; //message_t *msg;  
  uint8_t  txCount;
} fe_queue_entry_t;

/**
 * This structure is used by the routing engine to store
 *  the routing table.
 */
typedef struct {
  uint16_t    backpressure;
  uint16_t    linkPacketTxTime; // Exponential moving average in 100US units
  uint16_t    linkETX;          // Exponential moving average of ETX (in 100ths of expected transmissions)
  #ifndef BEACON_ONLY 
  uint8_t     lastTxNoStreakID; // Used to detect bursts of 3 successful receptions from a neighbor
  uint8_t     txNoStreakCount;  // Used to detect bursts of 3 successful receptions from a neighbor
  bool        isBurstyNow;      // Indicates whether the neighbor has notified of current "goodness" of link
  #endif
  rimeaddr_t  neighbor;
} routing_table_entry;

/**
 * This structure is used to track the last
 *  <source, packetID, hopCount> triplet received
 *  from a given neighbor.
 */
typedef struct{
  rimeaddr_t  neighbor;
  rimeaddr_t  origin;
  uint8_t     originSeqNo;
  uint8_t     hopCount;
} latestForwarded_table_entry;

//Errors if we need them
typedef uint8_t error_t;
enum {
  SUCCESS = 0, 
  FAIL = 1, 
  ESIZE = 2, 
  EOFF = 3, 
  EBUSY = 4
};

#endif /* __BCP_H__ */