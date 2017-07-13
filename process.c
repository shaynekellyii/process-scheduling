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
#define TERMINAL_FD		0
#define BUF_SIZE		500
#define NUM_SEMAPHORES	5
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
static void ProcInfo(int pid);
static void TotalInfo();

static void SelectNewRunningProcess();
static void PrintHelp();
static void AddProcessToReadyQueue(PROCESS *process);
static int PidComparator(void *proc1, void *proc2);
static PROCESS *GetProcByPid(int pid);
static int FindProcByPidAndDelete(int pid);

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

	printf("The INIT process will be created...\n");
	if (Create(INIT) < 0) {
		return -1;
	}
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
		printf("Attempted to fork the init process. Fork failed.\n\n");
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
	printf("High priority processes: ");
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

	printf("Normal priority processes: ");
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

	printf("Low priority processes: ");
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

	printf("Blocked processes: ");
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

	if (runningProcess != NULL) {
		printf("Running process - PID: %d, Priority: %s\n", 
			runningProcess->pid, PRIORITIES[runningProcess->priority]);
	} else {
		printf("Running process: NONE\n");
	}
	printf("Init process - PID: %d, Priority: %s\n", 
		initProcess->pid, PRIORITIES[initProcess->priority]);
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
			process->state = RUNNING;
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