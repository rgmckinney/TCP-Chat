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

#include "cclient.h"
#include "server.h"

#define MAXBUF 4000
#define INSTBUF 3

/**
 * By Ryan McKinney
 * TCP Chat Client
 */

// This clients handle
char* handle;

void getInput(int socket_num) {
	char inst[MAXBUF];
	fd_set fds;

	do {
		FD_ZERO(&fds);
		FD_SET(socket_num, &fds);
		FD_SET(STDIN_FILENO, &fds);
		
		printf("$: ");
		fflush(stdout);

		if (select(socket_num+1, &fds, NULL, NULL, NULL) < 0) {
			perror("select call");
		}

 		// Check for user instruction
 		if (FD_ISSET(STDIN_FILENO, &fds)) {
	 		fgets(inst, MAXBUF, stdin);

	 		if (inst[0] != '%') {
		 		printf("Invalid command\n");
	 		}
	 		else {
		 		switch(inst[1]) {
			 		case 'H':
			 		case 'h':
			 			// Help
			 			printHelp();
			 			break;
					case 'M':
			 		case 'm':
				 		// Send message
				 		sendMsg(socket_num, inst); 
				 		break;
			 		case 'B':
			 		case 'b':
				 		// Broadcast message
				 		sendBCast(socket_num, inst);
						break;
			 		case 'L':
			 		case 'l':
				 		// List Handles
				 		sendListReq(socket_num);
						break;
			 		case 'E':
			 		case 'e':
				 		// Exit
				 		sendExit(socket_num);
						break;
			 		default:
				 		printf("Invalid command\n");
				 		break;
		 		}
	 		}
		}

		// Check for incoming packet
		if (FD_ISSET(socket_num, &fds)) {
			receivePacket(socket_num);
		}
	} while (1);
}

void sendInitial(int socket_num) {
	struct c_header header;
	header.flag = F_INIT;
	u_short handleLen = getLength(handle);
	header.length = sizeof(struct c_header) + handleLen;

	// Send F_INIT to server
	char *packet = malloc(header.length);
	memcpy(packet, &header, sizeof(struct c_header));
	memcpy(packet + sizeof(struct c_header), handle, handleLen);

	send(socket_num, packet, MAXBUF, 0);

	// Wait for F_INIT_GOOD response from server
	char buffer[MAXBUF];
	c_header *rcvHeader;
	do {
		recv(socket_num, buffer, MAXBUF, MSG_WAITALL);
		rcvHeader = (c_header*)buffer;
		
		if (rcvHeader->flag == F_INIT_BAD) {
			printf("Handle already in use: %s\n", handle);
			exit(-1);
		}
	} while (rcvHeader->flag != F_INIT_GOOD);

	return;
}

void sendMsg(int socket_num, char inst[]) {
	short packetLength = sizeof(c_header);
	char* destHandles[MAXHNDL];
	u_char destHandleLens[MAXHNDL];

	// Construct packet header
	struct c_header header;
	header.flag = F_MSG;
	
	// Get client handle length
	u_char cHandleLen = (u_char)getLength(handle);
	packetLength += cHandleLen + sizeof(u_char);

	// Parse instruction
	strtok(inst, " ");

	char* nextInst = strtok(NULL, " ");
	if (nextInst == NULL) {
		printf("Invalid command\n");
		return;
	}
	
	u_char numHandles = atoi(nextInst);
	packetLength += sizeof(u_char);

	if (numHandles == 0) {
		// No handle num option given
		numHandles = 1;
		destHandleLens[0] = getLength(nextInst);
		packetLength += destHandleLens[0] + sizeof(u_char);
		destHandles[0] = nextInst;
	}
	else {
		int i;
		for (i=0; i < numHandles; i++) {
			nextInst = strtok(NULL, " ");
			if (nextInst == NULL) {
				printf("Invalid command\n");
				return;
			}
			destHandleLens[i] = getLength(nextInst);
			destHandles[i] = nextInst;
			packetLength += destHandleLens[i] + sizeof(u_char);
		}
	}
	
	// Get text of the message
	char* text = strtok(NULL, "\0");
	if (text == NULL) {
		printf("Invalid command\n");
		return;
	}
	packetLength += getLength(text);
	header.length = packetLength;
	
	// Construct message packet
	char* packet = createMsgPacket(packetLength, destHandles, destHandleLens, 
		text, numHandles);
	send(socket_num, packet, MAXBUF, 0);
}

void sendBCast(int socket_num, char inst[]) {
	strtok(inst, " ");
	char *text = strtok(NULL, "\0");

	char *packet = malloc(MAXBUF);
	char *current = packet + sizeof(c_header);

	// Construct header
	c_header *header = (c_header*)packet;
	header->flag = F_BCAST;

	// Construct sender handle
	u_char cLength = getLength(handle);
	memcpy(current, &cLength, sizeof(u_char));
	current += sizeof(u_char);
	memcpy(current, handle, cLength);
	current += cLength;
	
	header->length = sizeof(c_header) + sizeof(u_char) + cLength + getLength(text);
	
	memcpy(current, text, getLength(text));
	send(socket_num, packet, MAXBUF, 0);
}

void sendExit(int socket_num) {
	char *packet = malloc(MAXBUF);

	// Send exit packet to server
	c_header *header = (c_header*)packet;
	header->flag = F_EXIT;
	header->length = sizeof(c_header);
	send(socket_num, packet, MAXBUF, 0);

	// Wait for ACK from server
	recv(socket_num, packet, MAXBUF, MSG_WAITALL);
	while (header->flag != F_ACK) {
		recv(socket_num, packet, MAXBUF, MSG_WAITALL);
	}

	// Exit client
	exit(0);
}

void sendListReq(int socket_num) {
	// Send list request flag to server
	c_header *header = malloc(sizeof(c_header));
	header->flag = F_LIST;
	header->length = sizeof(c_header);
	send(socket_num, header, MAXBUF, 0);

	receiveList(socket_num);
}

char* createMsgPacket(short length, char* destHandles[], u_char handleLens[], 
	char* text, u_char numHandles) {
	char* packet = malloc(MAXBUF);
	char* current = packet;

	// Construct packet header
	c_header header;
	header.flag = F_MSG;
	header.length = length;
	memcpy(current, &header, sizeof(c_header));
	current += sizeof(c_header);

	// Construct sender handle section
	u_char cLength = getLength(handle);
	memcpy(current, &cLength, sizeof(u_char));
	current += sizeof(u_char);
	memcpy(current, handle, cLength);
	current += cLength;

	memcpy(current, &numHandles, sizeof(u_char));
	current += sizeof(u_char);

	// Construct destination handle sections
	int i;
	for (i=0; i < numHandles; i++) {
		u_char dLength = handleLens[i];
		memcpy(current, &dLength, sizeof(u_char));
		current += sizeof(u_char);
	
		memcpy(current, destHandles[i], dLength);
		current += dLength;
	}

	// Construct text section
	memcpy(current, text, getLength(text));

	return packet;
}

void receiveList(int socket_num) {
	char packet[MAXBUF];
	
	// Receive inc packet rom server
	recv(socket_num, packet, MAXBUF, MSG_WAITALL);
	c_header *header = (c_header*) packet;

	while (header->flag != F_INC) {
		recv(socket_num, packet, MAXBUF, MSG_WAITALL);
	}

	int numIncHandles = *(packet + sizeof(c_header));
	printf("Number of clients: %d\n", numIncHandles);

	// Receive rest of packets
	while (numIncHandles--) {
		recv(socket_num, packet, MAXBUF, MSG_WAITALL);
		u_char curLength = *(packet + sizeof(c_header));
		printf("  %.*s\n", curLength, packet+sizeof(c_header)+sizeof(u_char));
	}
}

void receiveBCast(char* packet) {
	c_header *header = (c_header*) packet;
	char *current = packet + sizeof(c_header);

	u_char cLength = *(current);
	current += sizeof(u_char);

	printf("\n%.*s: ", cLength, current);
	current += cLength;
	
	printf("%.*s", (int)(header->length - sizeof(c_header) - sizeof(u_char) - 
		cLength), current);
	
	free(packet);
}

void receiveMsg(char* packet) {
	char *inc_handle;
	u_char inc_handle_len, cur_handle_len;
	c_header *header = (c_header*)packet;
	int ofs = sizeof(c_header);

	// Get incoming handle
	inc_handle_len = *(packet + ofs);
	ofs += sizeof(u_char);
	inc_handle = packet + ofs;
	ofs += inc_handle_len;

	// Skip destination handles
	u_char numHandles = *(packet + ofs);
	ofs += sizeof(u_char);
	
	int i;
	for (i=0; i < numHandles; i++) {
		cur_handle_len = *(packet + ofs);
		ofs += sizeof(u_char) + cur_handle_len;
	}

	int textLength = header->length - ofs;
	printf("\n%.*s: ", inc_handle_len, inc_handle);
	printf("%.*s", textLength, packet + ofs);
}

void receiveError(char* packet) {
	u_char handleLength;
	c_header *header = (c_header*)packet;
	handleLength = header->length - sizeof(c_header) - sizeof(u_char);

	printf("\nClient with handle %.*s does not exist\n", handleLength, 
		packet+sizeof(c_header)+sizeof(u_char));
}

void receivePacket(int inc_socket) {
	char *packet = malloc(MAXBUF);
	recv(inc_socket, packet, MAXBUF, MSG_WAITALL);

	c_header *header = (c_header*)packet;
	switch(header->flag) {
		case F_MSG:
			receiveMsg(packet);
			break;
		case F_BCAST:
			receiveBCast(packet);
			break;
		case F_ERROR:
			receiveError(packet);
			break;
		default:
			printf("\nServer Terminated\n");
			exit(-1);
	}
}

int getLengthInput(char* data) {
	int length = 0;

	while (*data != '\n') {
		data++;
		length++;
	}

	return length;
}

u_char getLength(char* data) {
	if (data == NULL) {
		return 0;
	}
	
	u_char length = 0;
	while (*data != '\0') {
		data++;
		length++;
	}

	return length;
}

int setupClient(char *host_name, char *port) {
	int socket_num;
	struct sockaddr_in remote;       // socket address for remote side
	struct hostent *hp;        

	// create the socket
	if ((socket_num = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket call");
		exit(-1);
	}
	

	remote.sin_family= AF_INET;

	// get the address of the remote host and store
	if ((hp = gethostbyname(host_name)) == NULL) {
		printf("Error getting hostname: %s\n", host_name);
		exit(-1);
	}
	
	memcpy((char*)&remote.sin_addr, (char*)hp->h_addr, hp->h_length);

	// get the port used on the remote side and store
	remote.sin_port= htons(atoi(port));

	if(connect(socket_num, (struct sockaddr*)&remote, 
	 	sizeof(struct sockaddr_in)) < 0) {
		perror("connect call");
		exit(-1);
	}

	return socket_num;
}

void printHelp() {
   printf("\n   Message one client:\n      %%M destination-handle {message}\n");
   printf("   Message multiple clients:\n      %%M num-handles dest-handle-1 dest-handle-2 ... {message}\n");
   printf("   Broadcast a message:\n      %%B {message}\n");
   printf("   List clients in chat\n      %%L\n");
   printf("   Exit:\n      %%E\n");
}

int main(int argc, char * argv[]) {
	handle = argv[1];
	
	if (argc != 4) {
		printf("usage: %s handle host-name port-number \n", argv[0]);
		exit(1);
	}

	// Set up the server
	int socket_num = setupClient(argv[2], argv[3]);
	sendInitial(socket_num);
	
	printf("Type %%H for help\n");
	
	// Get user input
	getInput(socket_num);

	return 0;
}


