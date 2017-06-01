CXX=g++
CC=gcc

TARGET=umapsort

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

UFFDLOC = ../../umap
UFFDLD= -lrt -lssl -lcrypto
#CFLAGS=-std=gnu99 $(DEFS) -g -lpthread # -O3
CFLAGS=-std=gnu99 $(DEFS) -g -lpthread
HEADERS=$(UFFDLOC)/umap.h
TARGETOBJ=$(TARGET).o umap.o

all: $(TARGET)

$(TARGET): $(TARGETOBJ) 
	$(LINK.cpp) $(TARGETOBJ) -DUSEFILE $(LDFLAGS) $(UFFDLD) -o $@
umap.o: $(HEADERS) $(UFFDLOC)/umap.cpp
	$(CXX) $(UFFDLOC)/umap.cpp $(CXXFLAGS) -DUSEFILE -c -o $@
$(TARGET).o: $(TARGET).cpp
	$(CXX) $< $(CXXFLAGS) -c -o $@
clean:
	rm -f $(TARGET) $(TARGET).o umap.o
