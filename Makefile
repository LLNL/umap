CC=gcc
CFLAGS=-std=gnu99 -O3 -g -pthread -fopenmp -lrt
TARGET=hello
all: $(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).o
