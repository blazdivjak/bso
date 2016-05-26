ifndef TARGET
TARGET=z1
endif

ifndef CONTIKI_PROJECT
CONTIKI_WITH_RIME = 1
endif

ifndef PROJECT_NAME
PROJECT_NAME = gateway
endif

CONTIKI = /home/user/contiki
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
GATEWAY_LIB_DIR = ../lib

all:
	$(MAKE) clean -C $(GATEWAY_LIB_DIR)
	$(MAKE) -C $(GATEWAY_LIB_DIR)
	$(MAKE) $(PROJECT_NAME)

# TODO: add all needed libs as below
TARGET_LIBFILES += ../lib/a/libsensors.a
TARGET_LIBFILES += ../lib/a/libmessage.a
TARGET_LIBFILES += ../lib/a/libmath.a


include $(CONTIKI)/Makefile.include


lib:
	$(MAKE) -C $(GATEWAY_LIB_DIR)


upload:
	$(MAKE) clean -C $(GATEWAY_LIB_DIR)
	$(MAKE) -C $(GATEWAY_LIB_DIR)
	$(MAKE) $(PROJECT_NAME).upload

cleanall:
	$(MAKE) clean -C $(GATEWAY_LIB_DIR)
	$(MAKE) clean
