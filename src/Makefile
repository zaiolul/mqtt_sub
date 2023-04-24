
BIN:=mqtt_sub
SRCS:=$(wildcard *.c)
OBJS:=$(SRCS:.c=.o)

LIB:= -largp -lmosquitto -luci -ljson-c -lcurl -lsqlite3
#LDFLAGS:=-L../sdk/build/lib $(LIB)
INC:= -I./includes
 #-I../sdk/utils -I../sdk/include -I../sdk/interface -I../sdk/libraries/coreJSON/source/include 
CC:=gcc
CFLAGS = -Wall -g

.PHONY: all clean

all:$(BIN)
	
$(BIN): $(SRCS)
	$(CC) -o $@ $^ $(INC) $(LIB)

clean:
	rm $(BIN) 