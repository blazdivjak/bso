CC=msp430-gcc
AR=msp430-ar
MMCU=msp430f2617
CFLAGS=
LDFLAGS=
SOURCES = $(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
LIB_A=$(SOURCES:.c=.a)
OBJ_DIR=obj
A_DIR=a


all: prepare $(SOURCES) $(LIB_A)

%.o : %.c
	$(CC) -mmcu=%(MMCU) -c $(OBJ_DIR)/$@

%.a : %.o
	$(AR) -cvq $(A_DIR)/$@ $(OBJ_DIR)/$<

prepare:
	$(MAKE) -C $(OBJ_DIR) $(A_DIR)

clean:
	-rm $(OBJ_DIR)/* $(A_DIR)/*