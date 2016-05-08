ifndef TARGET
TARGET=z1
endif

ifndef CONTIKI_PROJECT
CONTIKI_WITH_RIME = 1
endif

ifndef PROJECT_NAME
PROJECT_NAME = gateway
endif

KRAVA_LIB_DIR = ../lib

all:
	$(MAKE) -C $(KRAVA_LIB_DIR)
	$(MAKE) $(PROJECT_NAME)

CONTIKI = /home/user/contiki

# TODO: add all needed libs as below
TARGET_LIBFILES += ../lib/a/libsensors.a
TARGET_LIBFILES += ../lib/a/libmessage.a


include $(CONTIKI)/Makefile.include


lib:
	$(MAKE) -C $(KRAVA_LIB_DIR)


upload:
	$(MAKE) -C $(KRAVA_LIB_DIR)
	$(MAKE) $(PROJECT_NAME).upload

cleanall:
	$(MAKE) clean -C $(KRAVA_LIB_DIR)
	$(MAKE) clean
