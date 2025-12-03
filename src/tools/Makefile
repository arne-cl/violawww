# Makefile for tools
#
# Build standalone utilities

CC = gcc
CFLAGS = -Wall -O2

TOOLS = iob2off

all: $(TOOLS)

iob2off: iob2off.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TOOLS)

.PHONY: all clean

