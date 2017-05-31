CC=gcc

TARGET=umaptest

DEFS=-D_GNU_SOURCE

ifdef D
  SEP := ,
  DEFS += $(patsubst %,-D%,$(subst $(SEP), ,$(D)))
endif

CFLAGS=-std=gnu99 $(DEFS) -O3 -g -lpthread -fopenmp -lrt -lssl -lcrypto

LDFLAGS=-lpthread -fopenmp -lrt -lssl -lcrypto
LDLIBS=-lssl -lcrypto

HEADERS=umap.h
SOURCES=umap.c test.c

OBJS=$(SOURCES:.c=.o)

$(TARGET): $(OBJS)

$(OBJS): $(SOURCES) $(HEADERS) Makefile

clean:
	rm -f $(TARGET) $(OBJS)
