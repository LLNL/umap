CXX=g++
CC=gcc

TARGET=mmapsort

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
UFFDLOC = ../uffd_handler
UFFDLD= -lrt -lssl -lcrypto
CFLAGS=-std=gnu99 $(DEFS) -O3 -g -lpthread
HEADERS=$(UFFDLOC)/uffd_handler.h
else
HEADERS=
SOURCES=$(TARGET).cpp
UFFDLD=
endif

SOURCES+=$(TARGET).cpp

OBJS+=$(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(TARGET).o uffd_handler.o
	$(LINK.cpp) $^ $(LDFLAGS) $(UFFDLD) -o $@
uffd_handler.o: $(HEADERS)
	$(CC) $(UFFDLOC)/uffd_handler.c $(CFLAGS) -c -o $@
$(TARGET).o: $(TARGET).cpp $(HEADERS)
	$(CXX) $< $(CXXFLAGS) -c -o $@
clean:
	rm -f $(TARGET) $(OBJS) uffd_handler.o
