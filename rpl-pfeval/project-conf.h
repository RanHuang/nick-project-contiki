#ifndef PROJECT_CONF_H
#define PROJECT_CONF_H

/**Configuration**/
#define UDP_SENDER_PORT 7777
#define UDP_SINK_PORT 8888

#ifndef SEND_INTERVAL
#define SEND_INTERVAL (10 * CLOCK_SECOND)
#endif
#define MAX_PAYLOAD_LEN 30

struct app_msg{
    uint16_t seqno;
    uint16_t parent_etx;
    uint16_t rtmetric;
    uint16_t num_neighbors;
    uint16_t beacon_interval;
    uint16_t hop_count;
    uint16_t lifetime;
    uint16_t data[10];
};
/****FOR DEBUG******/
/*
#define INFOPRINT 1
#if INFOPRINT
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define ERROR 1
#if ERROR
#include <stdio.h>
#define PRINT_ERROR(error, ...)\
{\
    if(error)\
    {\
        fprintf(stderr, "%s %s [%d]:", __FILE__, __FUNCTION__, __LINE__);\
        fprintf(stderr, __VA_ARGS__);\
    }\
}*/     /*The function cannot be used in the sky platform.*/
/*
#else 
#define PRINT_ERROR(error, ...)
#endif
*/
#define MACRO(s, ...) printf(s, ##__VA_ARGS__)

#endif
