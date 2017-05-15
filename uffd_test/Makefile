CXX=g++
CC=gcc

TARGET=uffd_test2

DEFS=-D_GNU_SOURCE

ifdef D
  SEP := ,
  DEFS += $(patsubst %,-D%,$(subst $(SEP), ,$(D)))
endif

CXXFLAGS= $(DEFS) -std=c++11  -g -lpthread -fopenmp # -O3

LDFLAGS=-lpthread -fopenmp

OS := $(shell uname)
ifneq ($(OS), Darwin)
LDFLAGS+= -lrt
endif

UFFDLOC = ../uffd_handler
UFFDLD= -lrt -lssl -lcrypto
CFLAGS=-std=gnu99 $(DEFS) -g -lpthread # -O3
HEADERS=$(UFFDLOC)/uffd_handler.h
TARGETOBJ=$(TARGET).o uffd_handler.o

all: $(TARGET)

$(TARGET): $(TARGETOBJ)
	$(LINK.cpp) $(TARGETOBJ) $(DEFS) $(LDFLAGS) $(UFFDLD) -o $@
uffd_handler.o: $(HEADERS)
	$(CC) $(UFFDLOC)/uffd_handler.c $(CFLAGS) $(DEFS) -c -o $@
$(TARGET).o: $(TARGET).cpp
	$(CXX) $< $(CXXFLAGS) -c -o $@
clean:
	rm -f $(TARGET) $(TARGET).o uffd_handler.o
