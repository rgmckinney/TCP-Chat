typedef struct __attribute__((packed)) c_header {
	short length;
	char flag;
} c_header;

void getInput(int socket_num);

void sendInitial(int socket_num);

void sendMsg(int socket_num, char inst[]);

void sendBCast(int socket_num, char inst[]);

void sendListReq(int socket_num);

void sendExit(int socket_num);

char* createMsgPacket(short length, char* destHandles[], u_char handleLens[], 
	char* text, u_char numHandles);
	
void receiveList(int socket_num);

void receiveBCast(char* packet);

void receiveMsg(char* packet);

void receiveError(char* packet);

void receivePacket(int inc_socket);

int getLengthInput(char* data);

u_char getLength(char* data);

void printData();

int setupClient(char *host_name, char *port);
