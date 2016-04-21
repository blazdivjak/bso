ifndef TARGET
TARGET=z1
endif

ifndef CONTIKI_PROJECT
CONTIKI_WITH_RIME = 1
endif

KRAVA_LIB_DIR = ../lib
.PHONY: lib_compile

CONTIKI_PROJECT = assignment3
all: $(CONTIKI_PROJECT1)

CONTIKI = /home/user/contiki
include $(CONTIKI)/Makefile.include

TARGET_LIBFILES += -L./../lib/a/ -lsensors


lib_compile:
	$(MAKE) -C $(KRAVA_LIB_DIR)