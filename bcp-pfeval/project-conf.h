#ifndef __PROJECT_CONF_H__
#define __PROJECT_CONF_H__

#ifndef PERIOD
#define PERIOD 4
#endif

#ifndef SEND_INTERVAL
#define SEND_INTERVAL (PERIOD * CLOCK_SECOND)
#endif

struct app_msg{
    uint16_t seqno;
    char data[10];
};

#endif

