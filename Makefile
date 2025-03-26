.PHONY: all build clean

CC=cc
CFLAGS=-Wall -Wextra -pedantic -ggdb
LIBS=-lncurses
EXEC=build/ncursnake

all: build

build:
	mkdir -p build
	$(CC) -o $(EXEC) src/ncursnake.c -Iinc $(LIBS) $(CFLAGS)

clean:
	rm -rf build

