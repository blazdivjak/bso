ifndef TARGET
TARGET=z1
endif

ifndef CONTIKI_PROJECT
CONTIKI_WITH_RIME = 1
endif



KRAVA_LIB_DIR = ../lib
.PHONY: lib_compile


all: 
	krava
	@echo $(value TARGET_LIBFILES)

CONTIKI = /home/user/contiki
TARGET_LIBFILES += -L. -lsensors
include $(CONTIKI)/Makefile.include

lib_compile:
	$(MAKE) -C $(KRAVA_LIB_DIR)