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

typedef struct SEMAPHORE {
	int value;
	LIST *blockedList;
} SEMAPHORE;

typedef struct PROCESS {
	PRIORITY priority;
	STATE state;
	int pid;
	char *msg;
	int msgIsReply;
} PROCESS;

typedef struct MSG {
	char *text;
	int pid;
} MSG;