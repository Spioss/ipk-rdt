CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -Werror -pedantic -O2
TARGET=ipk-rdt

SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

clean:
	rm -f $(OBJ) $(TARGET)
