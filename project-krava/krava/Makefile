ifndef TARGET
TARGET=z1
endif

ifndef CONTIKI_PROJECT
CONTIKI_WITH_RIME = 1
endif

ifndef PROJECT_NAME
PROJECT_NAME = krava
endif

CONTIKI = /home/user/contiki
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
KRAVA_LIB_DIR = ../lib

all:
	$(MAKE) clean -C $(KRAVA_LIB_DIR)
	$(MAKE) -C $(KRAVA_LIB_DIR)
	$(MAKE) $(PROJECT_NAME)

# TODO: add all needed libs as below
TARGET_LIBFILES += ../lib/a/libsensors.a
TARGET_LIBFILES += ../lib/a/libmessage.a
TARGET_LIBFILES += ../lib/a/libmath.a


include $(CONTIKI)/Makefile.include


lib:
	$(MAKE) -C $(KRAVA_LIB_DIR)


upload:
	$(MAKE) -C $(KRAVA_LIB_DIR)
	$(MAKE) $(PROJECT_NAME).upload

cleanall:
	$(MAKE) clean -C $(KRAVA_LIB_DIR)
	$(MAKE) rmo -C $(KRAVA_LIB_DIR)
	$(MAKE) clean
