/**
 * Structs
 */

typedef enum PRIORITY {
	HIGH,
	NORMAL,
	LOW,
	INIT
} PRIORITY;

typedef enum STATE {
	RUNNING,
	READY
} STATE;

typedef struct PROCESS {
	PRIORITY priority;
	STATE state;
	int pid;
} PROCESS;

/**
 * Function prototypes
 */