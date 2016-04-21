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


all: directories $(OBJECTS) $(LIB_A)

%.o: %.c
	$(CC) -mmcu=$(MMCU) -c $< -o $(OBJ_DIR)/$@

%.a: %.o
	$(AR) -cvq $(A_DIR)/$@ $(OBJ_DIR)/$<

directories: $(OBJ_DIR) $(A_DIR)
	
$(OBJ_DIR):
	$(MKDIR_P) $(OBJ_DIR)

$(A_DIR):
	$(MKDIR_P) $(A_DIR)

clean:
	-rm $(OBJ_DIR)/* $(A_DIR)/*

ifndef TARGET
TARGET=z1
endif
CONTIKI = /home/user/contiki
include $(CONTIKI)/Makefile.include