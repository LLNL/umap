CXX=g++
CC=gcc

TARGET=uffd_sort

DEFS=-D_GNU_SOURCE

ifdef D
  SEP := ,
  DEFS += $(patsubst %,-D%,$(subst $(SEP), ,$(D)))
endif

CXXFLAGS= $(DEFS) -std=c++11 -O3 -g -lpthread -fopenmp

LDFLAGS=-lpthread -fopenmp 

OS := $(shell uname)
ifneq ($(OS), Darwin)
LDFLAGS+= -lrt
endif

ifneq (,$(findstring UFFD,$DEFS))
UFFDOPTION=1
else 
UFFDOPTION=0
endif

ifeq ($(UFFDOPTION),1)
UFFDLOC = ../uffd_handler
UFFDLD= -lrt -lssl -lcrypto
CFLAGS=-std=gnu99 $(DEFS) -O3 -g -lpthread
HEADERS=$(UFFDLOC)/uffd_handler.h
TARGETOBJ=$(TARGET).o uffd_handler.o
else
HEADERS=
UFFDLD=
TARGETOBJ=$(TARGET).o
endif


all: $(TARGET)

$(TARGET): $(TARGETOBJ)
	$(LINK.cpp) $^ $(LDFLAGS) $(UFFDLD) -o $@
uffd_handler.o: $(HEADERS)
	$(CC) $(UFFDLOC)/uffd_handler.c $(CFLAGS) -c -o $@
$(TARGET).o: $(TARGET).cpp $(HEADERS)
	$(CXX) $< $(CXXFLAGS) -c -o $@
clean:
	rm -f $(TARGET) $(TARGET).o uffd_handler.o
