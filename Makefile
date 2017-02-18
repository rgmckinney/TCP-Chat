# Makefile for CPE464 

CC= gcc
CFLAGS= -g

# The  -lsocket -lnsl are sometimes needed for the sockets.
# The -L/usr/ucblib -lucb gives location for the Berkeley library needed for
# the bcopy, bzero, and bcmp.  The -R/usr/ucblib tells where to load
# the runtime library.

LIBS =

all:  cclient server

cclient: cclient.c cclient.h
	$(CC) $(CFLAGS) -o cclient -Wall cclient.c $(LIBS)

server: server.c server.h
	$(CC) $(CFLAGS) -o server -Wall server.c $(LIBS)

clean:
	rm -f server cclient *.o



