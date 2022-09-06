DESTDIR=./lib
SRCDIR=./src
TARGET=libpebs.so libpebs.a

CXX=g++
CXXFLAGS= -fopenmp -fPIC -O3 -Wall -std=c++14 -g -I./include
LDFLAGS=-lpfm -lnuma -lpthread -lboost_system -lboost_thread
LIB_OBJECTS := $(patsubst %.cc,%.o,$(wildcard $(SRCDIR)/*.cc))

all : $(TARGET)

$(TARGET) : clean $(LIB_OBJECTS)
	-mkdir -p $(DESTDIR)
	$(CXX) $(LIB_OBJECTS) -o $(DESTDIR)/$@ -shared 

clean:
	-rm -rf $(DESTDIR) $(SRCDIR)/*.o *~
