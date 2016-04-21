CC=msp430-gcc
AR=msp430-ar
MMCU=msp430f2617
CFLAGS=
LDFLAGS=
SOURCES = $(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
LIB_A=$(SOURCES:.c=.a)
MKDIR_P=mkdir -p
OBJ_DIR=obj
A_DIR=a


all: directories $(OBJECTS) $(LIB_A) rmo

%.o: %.c
	@echo [CC] $<
	$(CC) -mmcu=$(MMCU) -c $< -o $@

%.a: %.o
	@echo [AR] $<
	$(AR) -cvq $(A_DIR)/$@ $<

directories: $(A_DIR)

$(A_DIR):
	$(MKDIR_P) $(A_DIR)

rmo:
	-rm *.o

ifndef TARGET
TARGET=z1
endif
CONTIKI = /home/user/contiki
include $(CONTIKI)/Makefile.include

clean:
	-rm -rf $(A_DIR)