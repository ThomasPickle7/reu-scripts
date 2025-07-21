CC=gcc
CFLAGS=-Wall -Wextra -O2
TARGET=dma_test_app
SOURCES=main.c dma_driver.c test_suite.c
OBJECTS=$(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)


