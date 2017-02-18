#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "server.h"
#include "cclient.h"

#define MAXBUF 4000
#define MAXHNDL 100
#define MAXCONN 100

/**
 * By Ryan McKinney
 * TCP Chat Server
 */

int server_socket;
HandleNode *head = NULL;
HandleNode *tail = NULL;
int numHandles = 0;
fd_set allSockets;

void serve() {
	int client_socket;
	int last_socket = server_socket + 1;
	
	fd_set waiting;
	FD_SET(server_socket, &allSockets);

	if (listen(server_socket, MAXCONN) < 0) {
		perror("listen call");
		exit(-1);
	}

	while (1) {
		waiting = allSockets;
		if (select(last_socket, &waiting, NULL, NULL, NULL) < 0) {
			perror("select call");
			exit(-1);
		}

		// Server socket has new waiting client
		if (FD_ISSET(server_socket, &waiting)) {
			if ((client_socket = accept(server_socket, NULL, NULL)) < 0) {
				perror("accept call");
				exit(-1);
			}

			if (client_socket >= last_socket) {
				last_socket = client_socket + 1;
			}
			FD_SET(client_socket, &allSockets);
			serveClient(client_socket);
		}

		// Check client sockets for incoming packets
		int cur_socket;
		for (cur_socket=0; cur_socket < last_socket; cur_socket++) {
			if (cur_socket != server_socket && FD_ISSET(cur_socket, &waiting)) {
				serveClient(cur_socket);				
			}
		}
	}

	return;
}

void serveClient(int client_socket) {
	int length;
	char *buffer = malloc(MAXBUF);
	
	if ((length = recv(client_socket, buffer, MAXBUF, 0)) < 0) {
		perror("recv call");
		exit(-1);
	}

	struct c_header* header = (c_header*)buffer;	

	// Check incoming message flag
	switch (header->flag) {
		case F_INIT:
			serveInit(buffer, client_socket);
			break;
		case F_BCAST:
			serveBCast(buffer, client_socket);
			break;
		case F_MSG:
			serveMsg(buffer, client_socket);
			break;
		case F_LIST:
			serveList(client_socket);
			break;
		case F_EXIT:
			serveExit(client_socket);
			break;
		default:
			// Client disconnect
			removeClient(client_socket);
			FD_CLR(client_socket, &allSockets);
			numHandles--;
			break;
	}
}

void serveInit(char* packet, int inc_socket) {
	char *handle = packet + sizeof(struct c_header);
	c_header *inc_header = (c_header*)packet;

	if (isHandleOpen(handle)) {
		// Add handle to list
		HandleNode *newNode = malloc(sizeof(HandleNode));
		newNode->handle = handle;
		newNode->handleLength = inc_header->length - sizeof(c_header);
		newNode->socket = inc_socket;
		newNode->next = NULL;

		if (head == NULL) {
			head = newNode;
			tail = newNode;
		}
		else {
			tail->next = newNode;
			tail = tail->next;
		}
		
		numHandles++;

		// Send good flag
		c_header *header = malloc(MAXBUF);
		header->flag = F_INIT_GOOD;
		header->length = sizeof(c_header);

		// Set time until timeout	
		if (send(inc_socket, header, MAXBUF, 0) < 0) {
			perror("send error");
			exit(-1);
		}
	}
	else {
		// Send bad flag
		c_header *header = malloc(MAXBUF);
		header->flag = F_INIT_BAD;
		header->length = sizeof(c_header);

		send(inc_socket, header, MAXBUF, 0);
	}
}

void serveMsg(char* packet, int inc_socket) {
	char* current = packet + sizeof(c_header);
	char curHandle[MAXBUF];

	// Skip client handle info
	u_char cLength = (*current);
	current += sizeof(u_char) + cLength;

	// Get num handles
	u_char numHandles = (*current);
	current += sizeof(u_char);

	int i;
	for (i=0; i < numHandles; i++) {
		// Get destination handle info
		u_char dLength = (*current);
		current += sizeof(u_char);
	
		memcpy(curHandle, current, dLength);
		current += dLength;
		int dest_socket = getSocket(curHandle);
		if (dest_socket == -1) {
			sendErrorPacket(inc_socket, curHandle, dLength);
		}
		else {
			send(dest_socket, packet, MAXBUF, 0);
		}
	}

	free(packet);
}

void serveBCast(char* packet, int inc_socket) {
	// Forward packet to every socket but the incoming
	HandleNode *curNode = head;
	while (curNode != NULL) {
		if (curNode->socket != inc_socket) {
			send(curNode->socket, packet, MAXBUF, 0);
		}

		curNode = curNode->next;
	}
}

void serveList(int inc_socket) {
	char *packet = malloc(MAXBUF);
	
	// Send INC flag packet first
	c_header *header = (c_header*) packet;
	header->flag = F_INC;
	header->length = sizeof(c_header) + sizeof(int);
	memcpy(packet + sizeof(c_header), &numHandles, sizeof(int));
	send(inc_socket, packet, MAXBUF, 0);
	
	// Send one Handle packet per handle
	HandleNode *curNode = head;
	while (curNode != NULL) {
		header->flag = F_HNDL;
		header->length = sizeof(c_header) + sizeof(u_char) + curNode->handleLength;
		memcpy(packet + sizeof(c_header), &(curNode->handleLength), sizeof(u_char));
		memcpy(packet + sizeof(c_header) + sizeof(u_char), curNode->handle, 
			curNode->handleLength);
		send(inc_socket, packet, MAXBUF, 0);

		curNode = curNode->next;
	}

	free(packet);
}

void serveExit(int inc_socket) {
	numHandles--;
	removeClient(inc_socket);

	FD_CLR(inc_socket, &allSockets);

	// Send ACK flag to exiting client
	char *packet = malloc(MAXBUF);
	c_header *header = (c_header*)packet;
	header->flag = F_ACK;
	header->length = sizeof(c_header);
	send(inc_socket, packet, MAXBUF, 0);
	free(packet);
}

int isHandleOpen(char* handle) {
	HandleNode *curNode = head;
	while (curNode != NULL) {
		if (memcmp(handle, curNode->handle, curNode->handleLength) == 0) {
			return 0;
		}
		curNode = curNode->next;
	}

	return 1;
}

int getSocket(char* handle) {
	HandleNode *curNode = head;
	while (curNode != NULL) {
		if (memcmp(handle, curNode->handle, curNode->handleLength) == 0) {
			return curNode->socket;
		}
		curNode = curNode->next;
	}

	return -1;
}

void removeClient(int client_socket) {
	HandleNode *curNode = head;
	HandleNode *prevNode = NULL;
	while (curNode != NULL) {
		if (curNode->socket == client_socket) {
			if (prevNode == NULL) {
				head = curNode->next;
			}
			else {
				prevNode->next = curNode->next;
			}

			free(curNode);
		}

		prevNode = curNode;
		curNode = curNode->next;
	}
}

void sendErrorPacket(int socket_num, char *handle, u_char handleLength) {
	char *packet = malloc(MAXBUF);
	c_header *header = (c_header*)packet;
	header->flag = F_ERROR;
	header->length = sizeof(c_header) + sizeof(u_char) + handleLength;

	char *current = packet + sizeof(c_header);
	memcpy(current, &handleLength, sizeof(u_char));
	current += sizeof(u_char);

	memcpy(current, handle, handleLength);

	send(socket_num, packet, MAXBUF, 0);
}

int setupServer(int port) {
	int server_socket= 0;
	struct sockaddr_in local;
	socklen_t len= sizeof(local);

	// create the socket 
	server_socket= socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket < 0) {
		perror("socket call");
		exit(1);
	}

	local.sin_family= AF_INET;
	local.sin_addr.s_addr= INADDR_ANY; 
	local.sin_port= htons(port);                

	// bind the name (address) to a port 
	if (bind(server_socket, (struct sockaddr *) &local, sizeof(local)) < 0) {
		perror("bind call");
		exit(-1);
	}
	
	//get the port name and print it out
	if (getsockname(server_socket, (struct sockaddr*)&local, &len) < 0) {
		perror("getsockname call");
		exit(-1);
	}

	if (listen(server_socket, BACKLOG) < 0) {
		perror("listen call");
		exit(-1);
	}
	
	printf("server has port %d \n", ntohs(local.sin_port));
	
	return server_socket;
}

int main(int argc, char *argv[]) {
	int server_port = 0;

	if (argc > 2) {
		perror("usage: server <port-number>\n");
		exit(-1);
	}

	if (argc == 2) {
		server_port = strtol(argv[1], NULL, 10);
	}
	
	server_socket = setupServer(server_port);
	
	serve();

	return 0;
}
