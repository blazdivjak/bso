ifndef TARGET
TARGET=z1
endif

ifndef CONTIKI_PROJECT
CONTIKI_WITH_RIME = 1
endif

KRAVA_LIB_DIR = ../lib
.PHONY: lib_compile

lib_compile:
	$(MAKE) -C $(KRAVA_LIB_DIR)

CONTIKI = /home/user/contiki
TARGET_LIBFILES += ../lib/a/libsensors.a
include $(CONTIKI)/Makefile.include
