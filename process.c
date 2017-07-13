/***************************************************************
 * Process scheduling simulation				               *
 * Author: Shayne Kelly II                                     *
 * Date: July 5, 2017                                          *
 ***************************************************************/

/***************************************************************
 * Imports                                                     *
 ***************************************************************/
#include "process.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>

/***************************************************************
 * Defines and Constants                                       *
 ***************************************************************/
#define TERMINAL_FD		0
#define BUF_SIZE		500
#define NUM_SEMAPHORES	5
#define MAX_MSG_LEN		40
static const char * const PRIORITIES[4] = {"HIGH", "NORMAL", "LOW", "INIT"};
static const char * const STATES[5] = 
	{"RUNNING", "READY", "SEM BLOCKED", "SEND BLOCKED", "RECEIVE BLOCKED"};

/***************************************************************
 * Statics                                                     *
 ***************************************************************/
static LIST *highReadyQueue;
static LIST *normalReadyQueue;
static LIST *lowReadyQueue;
static LIST *blockedQueue;
static LIST *msgQueue;
static PROCESS *initProcess = NULL;
static PROCESS *runningProcess = NULL;
static SEMAPHORE *semaphoreArr[NUM_SEMAPHORES] = {NULL};
static char inputBuffer[BUF_SIZE];
static int nextAvailPid = 0;

/***************************************************************
 * Function Prototypes                                         *
 ***************************************************************/
static int Create(int priority);
static int Fork();
static int Kill(int pid);
static int Quantum();
static int NewSemaphore(int id, int value);
static void P(int id);
static void V(int id);
static void Send();
static void ProcInfo(int pid);
static void TotalInfo();

static void SelectNewRunningProcess();
static void PrintHelp();
static void AddProcessToReadyQueue(PROCESS *process);
static int PidComparator(void *proc1, void *proc2);
static PROCESS *GetProcByPid(int pid);
static int FindProcByPidAndDelete(int pid);
static void RemovePidFromBlockedQueue(PROCESS *process);
static PROCESS *SearchBlockedQueue(int pid);

/***************************************************************
 * Global Functions                                            *
 ***************************************************************/

int main(void) {
	printf("\n********** Welcome **********\n");

	/* Init ready queue list structures */
	printf("Initializing queues...\n");
	highReadyQueue = ListCreate();
	normalReadyQueue = ListCreate();
	lowReadyQueue = ListCreate();
	blockedQueue = ListCreate();
	msgQueue = ListCreate();

	printf("The INIT process will be created...\n");
	if (Create(INIT) < 0) {
		return -1;
	}
	initProcess->state = RUNNING;
	printf("********** Ready for commands **********\n\n");

	/* Infinite loop to read terminal commands */
	while (1) {
		char *pidChars = NULL;
		int pid = 0;

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
				printf("********** Create command issued **********\n");
				Create((int)(inputBuffer[2] - '0'));
				break;

			/* Exit */
			case 'e':
				/* fall-through */
			case 'E':
				printf("********** Exit command issued **********\n");
				Kill(runningProcess->pid);
				break;

			/* Fork */
			case 'f':
				/* fall-through */
			case 'F':
				printf("********** Fork command issued **********\n");
				Fork();
				break;

			/* Kill */
			case 'k':
				/* fall-through */
			case 'K':
				printf("********** Kill command issued **********\n");
				pidChars = inputBuffer + 2;
				pid = atoi(pidChars);
				Kill(pid);
				break;

			/* Quantum */
			case 'q':
				/* fall-through */
			case 'Q':
				printf("********** Quantum command issued **********\n");
				Quantum();
				break;

			/* New semaphore */
			case 'n':
				/* fall-through */
			case 'N':
				printf("********** New semaphore command issued **********\n");
				int semId = (int)(inputBuffer[2] - '0');
				char *semValChars = inputBuffer + 4;
				int semVal = atoi(semValChars);
				NewSemaphore(semId, semVal);
				break;

			/* P semaphore */
			case 'p':
				/* fall-through */
			case 'P':
				printf("********** Semaphore P command issued **********\n");
				P((int)(inputBuffer[2] - '0'));
				break;

			/* V semaphore */
			case 'v':
				/* fall-through */
			case 'V':
				printf("********** Semaphore V command issued **********\n");
				V((int)(inputBuffer[2] - '0'));
				break;

			/* Send */
			case 's':
				/* fall-through */
			case 'S':
				printf("********** Send command issued **********\n");
				Send();
				break;

			/* Procinfo */
			case 'i':
				/* fall-through */
			case 'I':
				printf("********** Process info command issued **********\n");
				pidChars = inputBuffer + 2;
				pid = atoi(pidChars);
				ProcInfo(pid);
				break;

			/* Totalinfo */
			case 't':
				/* fall-through */
			case 'T':
				printf("********** Process info command issued **********\n");
				TotalInfo();
				break;

			/* Help */
			case 'h':
				/* fall-through */
			case 'H':
				PrintHelp();
				break;

			/* Invalid command */
			default:
				printf("Invalid command entered. Try again.\n");
				break;
		} /* End of command switch statement */

		printf("********** Ready for next command **********\n\n");
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
		printf("ERROR - Invalid priority specified. Failed to create process.\n\n");
		free(process);
		return -1;
	} else {
		process->priority = priority;
	}

	process->pid = nextAvailPid;
	nextAvailPid++;
	process->state = READY;
	process->msg = NULL;

	AddProcessToReadyQueue(process);

	printf("Process created successfully.\n");
	printf("PID: %d\n", process->pid);
	printf("Added to %s priority (%d) ready queue.\n", 
		PRIORITIES[process->priority], process->priority);
	return process->pid;
}

/** 
 * Forks the currently running process.
 * Will fail on an attempt to fork the INIT process.
 * Returns PID of forked process on success, -1 on failure.
 */
static int Fork() {
	if (runningProcess == initProcess) {
		printf("Attempted to fork the init process. Fork failed.\n");
		return -1;
	}

	PROCESS *process = (PROCESS *)malloc(sizeof(PROCESS));
	process->pid = nextAvailPid;
	nextAvailPid++;
	process->state = READY;
	process->priority = runningProcess->priority;

	AddProcessToReadyQueue(process);

	printf("Process forked successfully.\n");
	printf("PID of forked process: %d\n", process->pid);
	printf("Added to %s priority ready queue.\n", PRIORITIES[process->priority]);
	return process->pid;
}

/** 
 * Forks the currently running process.
 * Will fail on an attempt to fork the INIT process.
 */
static int Kill(int pid) {	
	/* Check for attempt to kill INIT process */
	/* Terminate OS if only INIT process is left and it is killed */
	/* Otherwise keep the INIT process alive and return -1 */
	if (pid == 0) { 
		if (ListCount(highReadyQueue) == 0
				&& ListCount(normalReadyQueue) == 0
				&& ListCount(lowReadyQueue) == 0
				&& ListCount(blockedQueue) == 0) {
			printf("Killing the INIT process.\n");
			printf("No processes running.\n");
			printf("Terminating the OS. Goodbye.\n\n");
			exit(0);
		} else {
			printf("Can't kill the INIT process while other processes are in the OS.\n");
			return -1;
		}
	}

	/* Check if PID within the range of created PIDs */
	if (pid < 0 || pid >= nextAvailPid) {
		printf("Invalid PID specified.\n");
		return -1;
	}

	/* Check if PID is the running process */
	if (pid == runningProcess->pid) {
		printf("The killed process was the currently running process.\n");
		printf("The OS will select the next process to run.\n");

		/* Check queues in priority order to see which should run */
		SelectNewRunningProcess();
		return pid;
	}

	/* Search for PID in queues */
	if (FindProcByPidAndDelete(pid)) {
		return pid;
	} else {
		return -1;
	}
}

/** 
 * Pre-empts the running process and puts it back on the appropriate ready queue.
 * Starts running the highest priority process on a ready queue.
 * Returns the PID of the new running process, or -1 on failure.
 */
static int Quantum() {
	/* Pre-empt the running process and add it back to the appropriate queue */
	printf("Time quantum expired.\n");
	if (runningProcess != NULL && runningProcess->priority != INIT) {
		printf("Adding process PID %d back to %s priority ready queue.\n", 
			runningProcess->pid, PRIORITIES[runningProcess->priority]);
		AddProcessToReadyQueue(runningProcess);
	} else {
		printf("The running process was the INIT process. Not adding to ready queue...\n");
		initProcess->state = READY;
	}

	SelectNewRunningProcess();
	return runningProcess->pid;
}

/**
 * Creates a new semaphore with the supplied ID and value, 
 * if the semaphore with that ID hasn't been initialized.
 * Returns the semaphore ID on success, 0 on failure.
 */
static int NewSemaphore(int id, int value) {
	/* Check if semaphore ID is valid */
	if (id < 0 || id > 4) {
		printf("Invalid semaphore ID specified. ID must be a value between 0 and 4.\n");
		printf("Failed to initialize semaphore.\n");
		return 0;
	}

	/* Check if semaphore has already been initialized */
	if (semaphoreArr[id] != NULL) {
		printf("The semaphore with ID %d has already been initialized.\n", id);
		printf("Failed to initialize semaphore.\n");
		return 0;
	}

	/* Initialize the semaphore struct */
	SEMAPHORE *semaphore = (SEMAPHORE *)malloc(sizeof(SEMAPHORE));

	/* Check if semaphore value is appropriate and assign it */
	if (value >= 0) {
		semaphore->value = value;
	} else {
		printf("The semaphore value %d is invalid. It must be 0 or greater.\n", value);
		printf("Failed to initialize semaphore.\n");
		free(semaphore);
		return 0;
	}

	/* Initialize list of processes blocked on this semaphore */
	semaphore->blockedList = ListCreate();
	if (semaphore->blockedList == NULL) {
		printf("Failed to initialize the semaphore's blocked process list.\n");
		printf("Failed to initialize semaphore.\n");
		free(semaphore);
		return 0;
	}

	semaphoreArr[id] = semaphore;
	printf("Semaphore with ID %d and value %d created.\n", id, semaphore->value);
	return id;
}

/* Implements the semaphore P function */
static void P(int id) {
	/* Check if semaphore ID is valid */
	if (id < 0 || id >= NUM_SEMAPHORES) {
		printf("The semaphore ID %d is invalid. ID must be between 0-4.\n", id);
		printf("Failed to P on semaphore %d.\n", id);
		return;
	}

	/* Check if semaphore is initialized */
	if (semaphoreArr[id] == NULL) {
		printf("The semaphore with ID %d has not been initialized yet.\n", id);
		printf("Failed to P on semaphore %d.\n", id);
		return;
	}

	/* Check if there is a process running. Fail if there isn't. */
	if (runningProcess == NULL) {
		printf("There is no process currently running (all processes must be blocked).\n");
		printf("Failed to P on semaphore %d.\n", id);
		return;
	}

	SEMAPHORE *semaphore = semaphoreArr[id];
	semaphore->value--;
	printf("The semaphore value is now %d.\n", semaphore->value);
	
	/* Block the running process if the sem value is < 0 */
	if (semaphore->value < 0) {
		printf("Blocking the running process (PID %d).\n", runningProcess->pid);
		runningProcess->state = BLOCKED_SEM;
		ListAppend(blockedQueue, runningProcess);

		/* Add the process to the list of processes blocked on this sem */
		ListAppend(semaphore->blockedList, runningProcess);

		/* Select a new process to run */
		printf("Selecting a new process to run...\n");
		SelectNewRunningProcess();
	} else {
		printf("The semaphore value is still greater or equal to 0.\n");
		printf("The running process (PID %d) will not be blocked.\n", runningProcess->pid);
	}
}

/* Implements the semaphore V function */
static void V(int id) {
	/* Check if semaphore ID is valid */
	if (id < 0 || id >= NUM_SEMAPHORES) {
		printf("The semaphore ID %d is invalid. ID must be between 0-4.\n", id);
		printf("Failed to V on semaphore %d.\n", id);
		return;
	}

	/* Check if semaphore is initialized */
	if (semaphoreArr[id] == NULL) {
		printf("The semaphore with ID %d has not been initialized yet.\n", id);
		printf("Failed to V on semaphore %d.\n", id);
		return;
	}

	SEMAPHORE *semaphore = semaphoreArr[id];
	semaphore->value++;
	printf("The semaphore value is now %d.\n", semaphore->value);

	/* Wake up a blocked process if the sem value is <= 0 */
	if (semaphore->value <= 0) {
		printf("Waking up a process blocked on this semaphore.\n");
		PROCESS *procToWake = ListFirst(semaphore->blockedList);
		ListRemove(semaphore->blockedList);
		printf("This process has PID %d and priority %s.\n", 
			procToWake->pid, PRIORITIES[procToWake->priority]);
		RemovePidFromBlockedQueue(procToWake);
		AddProcessToReadyQueue(procToWake);
	} else {
		printf("The semaphore value is greater than 0.\n");
		printf("There are no blocked processes to wake up.\n");
	}
}

/** 
 * Send a message to the PID specified.
 * Both the message and PID are extracted from the user input buffer.
 */
static void Send() {
	/**
	 * Find the PID. Separate the PID and message by finding the space in between them 
	 * and null-terminating the end of the PID. 
	 */
	char *space = strchr(inputBuffer + 2, ' ');
	char *inputMsg = space + 1;
	*space = '\0';

	/* Parse out the PID and validate it. */
	int pid = atoi(inputBuffer + 2);
	if (pid < 0 || pid >= nextAvailPid) {
		printf("Invalid PID specified (%d).\n", pid);
		printf("Failed to send the message.\n");
		return;
	}

	/* Check if the message is a valid length. */
	if (strlen(inputMsg) > MAX_MSG_LEN) {
		printf("The message is too long. The max length is 40 characters.\n");
		printf("Failed to send the message.\n");
		return;
	} else if (!strlen(inputMsg)) {
		printf("Empty messages can't be sent.\n");
		printf("Failed to send the message.\n");
		return;
	}

	/* Build the message and add it to the inbox. */
	printf("Building the message to send to PID %d.\n", pid);
	MSG *msg = (MSG *)malloc(sizeof(MSG));
	char *msgContent = (char *)malloc(sizeof(char) * (strlen(inputMsg) + 1));
	strcpy(msgContent, inputMsg);
	msg->pid = pid;
	msg->text = msgContent;
	ListAppend(msgQueue, msg);

	/* Check if the process that the message will be sent to is already blocked on a receive. */
	/* Search the blocked queue for the destination PID. */
	PROCESS *rcvProcess = SearchBlockedQueue(pid);
	if (rcvProcess) {
		printf("The destination process was already waiting for a message.\n");
		
		/* Copy the message to the process */
		rcvProcess->msg = msgContent;

		/* Wake up the process */
		printf("Waking up the receiver process and placing it on the ready queue.\n");
		rcvProcess->state = READY;
		RemovePidFromBlockedQueue(rcvProcess);
		AddProcessToReadyQueue(rcvProcess);
	}

	/* Block the sending process until a reply is received. */
	printf("Blocking the sending process (PID %d) until a reply is received.\n", runningProcess->pid);
	runningProcess->state = BLOCKED_SEND;
	ListAppend(blockedQueue, runningProcess);

	/* Allow the next ready process to run. */
	printf("Selecting a new ready process to run.\n");
	SelectNewRunningProcess();
}

/* Prints all info about the process with the given PID parameter. */
static void ProcInfo(int pid) {
	printf("Requested info about process with PID %d:\n", pid);

	/* Check if PID is valid */
	if (pid < 0) {
		printf("The PID %d is invalid. It must be greater than 0.\n", pid);
		return;
	}

	/* Check if PID has been used */
	if (pid >= nextAvailPid) {
		printf("A process with PID %d has not been initialized yet.\n", pid);
		return;
	}

	PROCESS *process = NULL;
	/* Check if the process is running or the INIT process, or if it's in a queue */
	if (pid == 0) {
		process = initProcess;
	} else if (pid == runningProcess->pid) {
		process = runningProcess;
	} else {
		process = GetProcByPid(pid);
	}
	
	/* If the process pointer is NULL, it was deleted from the system */
	if (process == NULL) {
		printf("The process with PID %d was killed and removed from the OS.\n", pid);
		return;
	}

	/* Print out all available info about the process */
	printf("The process has priority %s.\n", PRIORITIES[process->priority]);
	printf("The process is currently in the %s state.\n", STATES[process->state]);
}

/* Prints status of all the process queues to the terminal */
static void TotalInfo() {
	printf("High priority processes in queue: ");
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

	printf("Normal priority processes in queue: ");
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

	printf("Low priority processes in queue: ");
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

	printf("Blocked processes in queue: ");
	if (ListCount(blockedQueue) == 0) {
		printf("NONE\n\n");
	} else {
		ListFirst(blockedQueue);
		for (int i = 0; i < ListCount(blockedQueue); i++) {
			PROCESS *process = (PROCESS *)ListCurr(blockedQueue);
			printf("%d, ", process->pid);
			ListNext(blockedQueue);
		}
		printf("\n\n");
	}

	if (runningProcess != NULL) {
		printf("Running process - PID: %d, Priority: %s, State: %s\n", 
			runningProcess->pid, PRIORITIES[runningProcess->priority], STATES[runningProcess->state]);
	} else {
		printf("Running process: NONE\n");
	}
	printf("Init process - PID: %d, Priority: %s, State: %s\n\n", 
		initProcess->pid, PRIORITIES[initProcess->priority], STATES[initProcess->state]);

	printf("Message queue count: %d\n", ListCount(msgQueue));
}

/***************************************************************
 * Helper Functions                                            *
 ***************************************************************/

/* Adds a process to the appropriate ready queue based on priority */
static void AddProcessToReadyQueue(PROCESS *process) {
	process->state = READY;
	switch (process->priority) {
		case HIGH:
			ListAppend(highReadyQueue, process);
			return;
		case NORMAL:
			ListAppend(normalReadyQueue, process);
			return;
		case LOW:
			ListAppend(lowReadyQueue, process);
			return;
		case INIT:
			initProcess = process;
			runningProcess = process;
			break;
		default:
			break;
	}
}

/* Checks the priorities queues to select which process to run next */
static void SelectNewRunningProcess() {
	/* Check queues in priority order to see which should run */
	if (ListCount(highReadyQueue) > 0) {
		printf("Getting new process from HIGH priority queue...\n");
		PROCESS *newRunningProc = ListFirst(highReadyQueue);
		ListRemove(highReadyQueue);
		runningProcess = newRunningProc;
		runningProcess->state = RUNNING;
		printf("The new running process has PID %d.\n", runningProcess->pid);
		return;
	}
	if (ListCount(normalReadyQueue) > 0) {
		printf("Getting new process from NORMAL priority queue...\n");
		PROCESS *newRunningProc = ListFirst(normalReadyQueue);
		ListRemove(normalReadyQueue);
		runningProcess = newRunningProc;
		runningProcess->state = RUNNING;
		printf("The new running process has PID %d.\n", runningProcess->pid);
		return;
	}
	if (ListCount(lowReadyQueue) > 0) {
		printf("Getting new process from LOW priority queue...\n");
		PROCESS *newRunningProc = ListFirst(lowReadyQueue);
		ListRemove(lowReadyQueue);
		runningProcess = newRunningProc;
		runningProcess->state = RUNNING;
		printf("The new running process has PID %d.\n", runningProcess->pid);
		return;
	}

	/* If there are no ready processes, let the INIT process run IF it's not blocked */
	printf("No processes on ready queues. Checking if the INIT process is ready...\n");
	if (initProcess->state == READY || initProcess->state == RUNNING) {
		printf("The INIT process is now running.\n");
		runningProcess = initProcess;
	} else {
		printf("The INIT process is blocked. No process is available to run.\n");
		runningProcess = NULL;
	}

	return;
}

/* Prints instructions for process manipulation to the terminal */
static void PrintHelp() {

}

/* Returns 0 if the processes PIDs don't match, 1 if the PIDs do match */
static int PidComparator(void *proc1, void *proc2) {
	PROCESS *process1 = (PROCESS *)proc1;
	PROCESS *process2 = (PROCESS *)proc2;
	return process1->pid == process2->pid;
}

/**
 * Returns a pointer to the process with the specified PID, if it is found.
 * Returns NULL if no process was found.
 */
static PROCESS *GetProcByPid(int pid) {
	PROCESS *procToFind = (PROCESS *)malloc(sizeof(PROCESS));
	procToFind->pid = pid;

	PROCESS *foundProc = NULL;
	ListFirst(highReadyQueue);
	foundProc = ListSearch(highReadyQueue, PidComparator, procToFind);
	if (foundProc != NULL) {
		return foundProc;
	}

	ListFirst(normalReadyQueue);
	foundProc = ListSearch(normalReadyQueue, PidComparator, procToFind);
	if (foundProc != NULL) {
		return foundProc;
	}

	ListFirst(lowReadyQueue);
	foundProc = ListSearch(lowReadyQueue, PidComparator, procToFind);
	if (foundProc != NULL) {
		ListRemove(lowReadyQueue);
		free(foundProc);
		return foundProc;
	}

	ListFirst(blockedQueue);
	foundProc = ListSearch(blockedQueue, PidComparator, procToFind);
	if (foundProc != NULL) {
		ListRemove(blockedQueue);
		free(foundProc);
		return foundProc;
	}

	return NULL;
}

/**
 * Searches the priority queues and blocked queues for a PID.
 * Returns the PID if found. Returns 0 if not found.
 */
static int FindProcByPidAndDelete(int pid) {
	PROCESS *procToFind = (PROCESS *)malloc(sizeof(PROCESS));
	procToFind->pid = pid;

	PROCESS *foundProc = NULL;
	ListFirst(highReadyQueue);
	foundProc = ListSearch(highReadyQueue, PidComparator, procToFind);
	if (foundProc != NULL) {
		ListRemove(highReadyQueue);
		free(foundProc);
		printf("Successfully killed process with PID %d\n", pid);
		return pid;
	}

	ListFirst(normalReadyQueue);
	foundProc = ListSearch(normalReadyQueue, PidComparator, procToFind);
	if (foundProc != NULL) {
		ListRemove(normalReadyQueue);
		free(foundProc);
		printf("Successfully killed process with PID %d\n", pid);
		return pid;
	}

	ListFirst(lowReadyQueue);
	foundProc = ListSearch(lowReadyQueue, PidComparator, procToFind);
	if (foundProc != NULL) {
		ListRemove(lowReadyQueue);
		free(foundProc);
		printf("Successfully killed process with PID %d\n", pid);
		return pid;
	}

	ListFirst(blockedQueue);
	foundProc = ListSearch(blockedQueue, PidComparator, procToFind);
	if (foundProc != NULL) {
		ListRemove(blockedQueue);
		free(foundProc);
		printf("Successfully killed process with PID %d\n", pid);
		return pid;
	}

	free(foundProc);
	printf("Failed to kill process with PID %d\n", pid);
	return 0;
}

/**
 * Searches the blocked queue for the process given, and removes it from the queue.
 */
static void RemovePidFromBlockedQueue(PROCESS *process) {
	ListFirst(blockedQueue);
	PROCESS *foundProc = ListSearch(blockedQueue, PidComparator, process);
	if (foundProc != NULL) {
		ListRemove(blockedQueue);
		printf("Successfully removed process with PID %d from the blocked queue.\n", process->pid);
	} else {
		printf("Failed to remove process with PID %d from the blocked queue.\n", process->pid);
	}
}

/**
 * Searches the blocked queue for the given PID.
 * Returns the process if found, NULL if not found.
 */
static PROCESS *SearchBlockedQueue(int pid) {
	PROCESS *process = (PROCESS *)malloc(sizeof(process));
	process->pid = pid;
	ListFirst(blockedQueue);
	PROCESS *foundProc = ListSearch(blockedQueue, PidComparator, process);
	free(process);
	return foundProc;
}