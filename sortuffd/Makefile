CXX=g++

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


HEADERS=uffd_handler.h
SOURCES=uffd_handler.c $(TARGET).cpp

all: $(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).o
