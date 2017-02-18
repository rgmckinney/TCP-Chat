#define F_INIT 0x01
#define F_INIT_GOOD 0x02
#define F_INIT_BAD 0x03
#define F_BCAST 0x04
#define F_MSG 0x05
#define F_ERROR 0x07
#define F_EXIT 0x08
#define F_ACK 0x09
#define F_LIST 0x0A
#define F_INC 0x0B
#define F_HNDL 0x0C

#define MAXHNDL 100
#define BACKLOG 5

typedef struct HandleNode {
	char* handle;
	u_char handleLength;
	int socket;
	struct HandleNode* next;
} HandleNode;

void printData(char *data, int len);
