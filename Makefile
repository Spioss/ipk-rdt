CC      = gcc
CFLAGS  = -std=c17 -Wall -Wextra -Wpedantic -O2 -D_POSIX_C_SOURCE=200809L
TARGET  = ipk-rdt
SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

test:
	$(CC) $(CFLAGS) -o tests/test_protocol src/protocol.c tests/test_protocol.c
	./tests/test_protocol
	rm -f tests/test_protocol

clean:
	rm -f $(OBJ) $(TARGET)
	rm -f tests/test_protocol