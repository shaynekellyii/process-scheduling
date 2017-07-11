/***************************************************************
 * Process scheduling simulation				               *
 * Author: Shayne Kelly II                                     *
 * Date: July 5, 2017                                          *
 ***************************************************************/

/***************************************************************
 * Imports                                                     *
 ***************************************************************/
#include "process.h"
#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>

/***************************************************************
 * Defines and Constants                                       *
 ***************************************************************/
#define TERMINAL_FD	0
#define BUF_SIZE	500
static const char * const PRIORITIES[4] = {"HIGH", "NORMAL", "LOW", "INIT"};

/***************************************************************
 * Statics                                                     *
 ***************************************************************/
static LIST *highReadyQueue;
static LIST *normalReadyQueue;
static LIST *lowReadyQueue;
static LIST *blockedQueue;
static PROCESS *initProcess = NULL;
static PROCESS *runningProcess = NULL;
static char inputBuffer[BUF_SIZE];
static int nextAvailPid = 0;

/***************************************************************
 * Function Prototypes                                         *
 ***************************************************************/
static int Create(int priority);
static int Fork();
static int Kill();
static void PrintHelp();
static void PrintQueueStatus();
static void AddProcessToReadyQueue(PROCESS *process);

/***************************************************************
 * Global Functions                                            *
 ***************************************************************/

int main(void) {
	/* Init ready queue list structures */
	printf("[INIT] Initializing queues...\n");
	highReadyQueue = ListCreate();
	normalReadyQueue = ListCreate();
	lowReadyQueue = ListCreate();
	blockedQueue = ListCreate();

	printf("[INIT] Creating init process...\n\n");
	if (Create(INIT) < 0) {
		return -1;
	}

	/* Infinite loop to read terminal commands */
	while (1) {
		read(TERMINAL_FD, inputBuffer, BUF_SIZE);

		/**
		 * Get the command from the first char of the input buffer.
		 * Switch statement covers all the available OS commands.
		 */
		switch (inputBuffer[0]) {

			/* Create */
			case 'c':
				/* fall-through */
			case 'C':
				Create((int)(inputBuffer[2] - '0'));
				break;

			/* Fork */
			case 'f':
				/* fall-through */
			case 'F':
				Fork();
				break;

			/* Kill */
			case 'k':
				/* fall-through */
			case 'K':
				Kill();
				break;

			/* Help */
			case 'h':
				/* fall-through */
			case 'H':
				PrintHelp();
				break;

			/* Invalid command */
			default:
				printf("[OS] Invalid command entered. Try again.\n\n");
				break;
		}
	}
}

/***************************************************************
 * Static Functions                                            *
 ***************************************************************/

/* Creates a new process and adds it to the appropriate ready queue */
/* TODO: Add failure handling - failure = return -1 */
static int Create(int priority) {
	PROCESS *process = (PROCESS *)malloc(sizeof(PROCESS));

	if ((priority == INIT && initProcess != NULL) || 
			(priority > INIT) || 
			(priority < HIGH)) {
		printf("[C] ERROR - Invalid priority specified. Failed to create process.\n\n");
		free(process);
		return -1;
	} else {
		process->priority = priority;
	}

	process->pid = nextAvailPid;
	nextAvailPid++;
	process->state = READY;

	AddProcessToReadyQueue(process);

	printf("[C] Process created successfully.\n");
	printf("[C] PID: %d\n", process->pid);
	printf("[C] Added to %s priority (%d) ready queue.\n", 
		PRIORITIES[process->priority], process->priority);
	PrintQueueStatus();
	return process->pid;
}

/** 
 * Forks the currently running process.
 * Will fail on an attempt to fork the INIT process.
 * Returns PID of forked process on success, -1 on failure.
 */
static int Fork() {
	if (runningProcess == initProcess) {
		printf("[F] Attempted to fork the init process. Fork failed.\n\n");
		return -1;
	}

	PROCESS *process = (PROCESS *)malloc(sizeof(PROCESS));
	process->pid = nextAvailPid;
	nextAvailPid++;
	process->state = READY;
	process->priority = runningProcess->priority;

	AddProcessToReadyQueue(process);

	printf("[F] Process forked successfully.\n");
	printf("[F] PID of forked process: %d\n", process->pid);
	printf("[F] Added to %s priority ready queue.\n", PRIORITIES[process->priority]);
	PrintQueueStatus();
	return process->pid;
}

/** 
 * Forks the currently running process.
 * Will fail on an attempt to fork the INIT process.
 */
static int Kill() {
	/* Parse PID from input buffer */
	char *pidChars = inputBuffer[2];
	int pid = atoi(pidChars);

	/* Check for attempt to kill INIT process */
	/* Terminate OS if only INIT process is left and it is killed */
	/* Otherwise keep the INIT process alive and return -1 */
	if (pid == 0) { 
		if (ListCount(highReadyQueue) == 0
				&& ListCount(normalReadyQueue) == 0
				&& ListCount(lowReadyQueue) == 0
				&& ListCount(blockedQueue) == 0) {
			printf("[K] Killing the INIT process.\n");
			printf("[K] No processes running.\n");
			printf("[K] Terminating the OS. Goodbye.\n\n");
			exit(0);
		} else {
			printf("[K] Can't kill the INIT process while other processes are in the OS.\n");
			PrintQueueStatus();
			return -1;
		}
	}

	/* Check if PID within the range of created PIDs */
	if (pid < 0 || pid >= nextAvailPid) {
		printf("[K] Invalid PID specified.\n");
		PrintQueueStatus();
		return -1;
	}

	/* Search for PID in queues */
	void *ListSearch(LIST *list, int (*comparator)(void *, void *), void *comparisonArg);
	PROCESS *process = NULL;
	ListSearch(highReadyQueue, )

	/* Delete it from the queue if found */
}

/***************************************************************
 * Helper Functions                                            *
 ***************************************************************/

/* Adds a process to the appropriate ready queue based on priority */
static void AddProcessToReadyQueue(PROCESS *process) {
	switch (process->priority) {
		case HIGH:
			ListAppend(highReadyQueue, process);
			break;
		case NORMAL:
			ListAppend(normalReadyQueue, process);
			break;
		case LOW:
			ListAppend(lowReadyQueue, process);
			break;
		case INIT:
			initProcess = process;
			runningProcess = process;
			process->state = RUNNING;
		default:
			break;
	}
}

/* Prints instructions for process manipulation to the terminal */
static void PrintHelp() {

}

/* Prints status of all the process queues to the terminal */
static void PrintQueueStatus() {
	printf("********** OS info **********\n");

	printf("[OS] High priority processes: ");
	if (ListCount(highReadyQueue) == 0) {
		printf("NONE\n");
	} else {
		ListFirst(highReadyQueue);
		for (int i = 0; i < ListCount(highReadyQueue); i++) {
			PROCESS *process = (PROCESS *)ListCurr(highReadyQueue);
			printf("%d, ", process->pid);
			ListNext(highReadyQueue);
		}
		printf("\n");
	}

	printf("[OS] Normal priority processes: ");
	if (ListCount(normalReadyQueue) == 0) {
		printf("NONE\n");
	} else {
		ListFirst(normalReadyQueue);
		for (int i = 0; i < ListCount(normalReadyQueue); i++) {
			PROCESS *process = (PROCESS *)ListCurr(normalReadyQueue);
			printf("%d, ", process->pid);
			ListNext(normalReadyQueue);
		}
		printf("\n");
	}

	printf("[OS] Low priority processes: ");
	if (ListCount(lowReadyQueue) == 0) {
		printf("NONE\n");
	} else {
		ListFirst(lowReadyQueue);
		for (int i = 0; i < ListCount(lowReadyQueue); i++) {
			PROCESS *process = (PROCESS *)ListCurr(lowReadyQueue);
			printf("%d, ", process->pid);
			ListNext(lowReadyQueue);
		}
		printf("\n");
	}

	printf("[OS] Blocked processes: ");
	if (ListCount(blockedQueue) == 0) {
		printf("NONE\n");
	} else {
		ListFirst(blockedQueue);
		for (int i = 0; i < ListCount(blockedQueue); i++) {
			PROCESS *process = (PROCESS *)ListCurr(blockedQueue);
			printf("%d, ", process->pid);
			ListNext(blockedQueue);
		}
		printf("\n");
	}

	printf("[OS] Running process - PID: %d, Priority: %s\n", 
		runningProcess->pid, PRIORITIES[runningProcess->priority]);
	printf("[OS] Init process - PID: %d, Priority: %s\n", 
		initProcess->pid, PRIORITIES[initProcess->priority]);

	printf("[OS] Ready for next command:\n\n");
}

static void PidComparator(void *proc1, void *proc2) {
	
}