include ../config.mk

CC ?= cc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2 -DNDEBUG
CFLAGS += -Wno-unused-result
LDFLAGS =

SRC_FILES = main.c
OBJ_FILES = $(SRC_FILES:.c=.o)

$(BIN): $(OBJ_FILES)
	$(CC) -o $(BIN) $(OBJ_FILES)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf $(BIN) *.o
