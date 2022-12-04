# Makefile for RCOM - Project 2

COMPILER_TYPE = gnu
CC = gcc

PROG = rc
SRCS = clientTCP.c

CFLAGS= -Wall -g

$(PROG): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(PROG)

clean:
	rm -f $(PROG)