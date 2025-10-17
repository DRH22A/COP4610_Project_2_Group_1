CC=gcc
CFLAGS=-O2 -Wall -Wextra -static-pie

all: empty part1 traces

empty: empty.c
	$(CC) $(CFLAGS) -o empty empty.c

part1: part1.c
	$(CC) $(CFLAGS) -o part1 part1.c

traces: empty part1
	strace -o empty.trace ./empty
	strace -o part1.trace ./part1

clean:
	rm -f empty part1 *.trace
