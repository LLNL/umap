CC=gcc

TARGET=hello

DEFS=-D_GNU_SOURCE

ifdef D
  SEP := ,
  DEFS += $(patsubst %,-D%,$(subst $(SEP), ,$(D)))
endif

CFLAGS=-std=gnu99 $(DEFS) -O3 -g -lpthread -fopenmp -lrt

all: $(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).o
