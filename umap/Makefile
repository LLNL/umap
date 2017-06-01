GXX=gcc
CC=gcc

TARGET=umaptest

DEFS=-D_GNU_SOURCE

ifdef D
  SEP := ,
  DEFS += $(patsubst %,-D%,$(subst $(SEP), ,$(D)))
endif

CFLAGS=   $(DEFS) -std=gnu99 -g -lpthread -fopenmp # -O3
CXXFLAGS= $(DEFS) -std=c++11 -g -lpthread -fopenmp # -O3
LDFLAGS=-lpthread -fopenmp

HEADERS=umap.h
SOURCES=umap.cpp test.c

OBJS=umap.o test.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LINK.cpp) $(OBJS) $(LDFLAGS) -o $@

$(OBJS): $(SOURCES) $(HEADERS) Makefile

clean:
	rm -f $(TARGET) $(OBJS)
