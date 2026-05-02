CC = gcc
CFLAGS  = -std=c17 -Wall -Wextra -Wpedantic -O2 -D_POSIX_C_SOURCE=200809L
TARGET = ipk-rdt

SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

clean:
	rm -f $(OBJ) $(TARGET)
