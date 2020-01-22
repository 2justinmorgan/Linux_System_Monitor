TARGET   = monitor
CC       = gcc
CCFLAGS  = -Wall 
LDFLAGS  = -lm
SOURCES  = monitor.c
INCLUDES = $(wildcard *.h)
OBJECTS  = $(SOURCES:.c=.o)

monitor: monitor.c

all: monitor 

$(TARGET):$(OBJECTS)
	$(CC) -o $(TARGET) $(LDFLAGS) $(OBJECTS)

$(OBJECTS):$(SOURCES) $(INCLUDES)
	$(CC) -c $(CCFLAGS) $(SOURCES)

clean:
	rm -f monitor *.o
