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
CFLAGS=-std=gnu99 $(DEFS) -O3 -g -lpthread -fopenmp -lrt -lssl -lcrypto
HEADERS=../uffd_handler/uffd_handler.h
SOURCES=../uffd_handler/uffd_handler.c 
OBJS=$(SOURCES:.c=.o)
else
HEADERS=
SOURCES=$(TARGET).cpp
endif

#SOURCES+=$(TARGET).cpp

OBJS+=$(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LINK.cpp) $^ $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET) $(OBJS)
