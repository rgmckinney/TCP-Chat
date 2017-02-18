CC= gcc
CFLAGS= -g

LIBS =

all:  cclient server

cclient: cclient.c cclient.h
	$(CC) $(CFLAGS) -o cclient -Wall cclient.c $(LIBS)

server: server.c server.h
	$(CC) $(CFLAGS) -o server -Wall server.c $(LIBS)

clean:
	rm -f server cclient *.o



