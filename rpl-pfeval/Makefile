CONTIKI = $(HOME)/contiki/
CONTIKI_PROJECT = sender sink

WITH_UIP6 = 1
UIP_CONF_IPV6 = 1
CFLAGS += -DUIP_CONF_IPV6_RPL

ifdef PERIOD
CFLAGS += -DPERIOD = $(PERIOD)
endif

all: $(CONTIKI_PROJECT)

include $(CONTIKI)/Makefile.include
