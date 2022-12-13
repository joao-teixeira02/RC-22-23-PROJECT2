COMPILER_TYPE = gnu
CC = gcc

PROG = download
SRCS = clientTCP.c

CFLAGS= -Wall -g

$(PROG): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(PROG)

clean:
	rm -f $(PROG)