/***************************************************************
 * Imports                                                     *
 ***************************************************************/
#include "list.h"

/***************************************************************
 * Structs                                                     *
 ***************************************************************/
typedef enum PRIORITY {
	HIGH,
	NORMAL,
	LOW,
	INIT
} PRIORITY;

typedef enum STATE {
	RUNNING,
	READY,
	BLOCKED_SEM,
	BLOCKED_SEND,
	BLOCKED_RCV
} STATE;

typedef enum MSG_TYPE {
	NEW,
	REPLY
} MSG_TYPE;

typedef struct SEMAPHORE {
	int value;
	LIST *blockedList;
} SEMAPHORE;

typedef struct MSG {
	char *text;
	int sendPid;
	int rcvPid;
	MSG_TYPE type;
} MSG;

typedef struct PROCESS {
	PRIORITY priority;
	STATE state;
	int pid;
	MSG *msg;
} PROCESS;