README for Process Scheduling Simulation
Author - Shayne Kelly II

*** Process structure ***

The Process Control Block contains the following:
- PRIORITY priority: enum of HIGH (0), NORMAL (1), LOW (2), and INIT priority
	- Processes cannot be created with INIT priority, it's for housekeeping
- STATE state: enum of RUNNING, READY, BLOCKED_SEM, BLOCKED_SEND, BLOCKED_RCV
	- INIT process cannot be blocked
- int pid: the assigned process ID
	- PIDs increment from 0 and are not reused
- MSG *msg: a struct that holds the latest received new message/reply from another process
	- Only the last message/reply sent to the process will be saved in the PCB 
	when the process wakes up (for simplicity)

*** Inputting commands ***

Commands must be input in the following format:
	[command] [arg1] [arg2]
Only a single space can be placed between the single letter command and any arguments.

*** Process creation/deletion ***

Creation:
	- Processes can be created with any priority from HIGH to LOW, 
	except INIT (reserved for the OS)
	- Reports:
		- If creating a process succeeded/failed
		- The PID of the new process
		- The priority queue it was added to

Deletion:
	- Kill command will kill any process, exit command kills the running process
	- The INIT process cannot be killed when other processes are running
		- Killing the INIT process with no other processes running will terminate the simulation
	- Reports:
		- If killing/exiting the process succeeded or failed

Forking:
	- Creates a copy of the running process (new process with the same priority)
	- The INIT process can't be forked
	- Reports:
		- If forking the running process succeeded/failed
		- The PID of the forked process
		- The priority queue it was added to

*** Process scheduling ***

The quantum command pre-empts the running process with a 3 priority level round-robin algorithm.
	- The first process in line on the highest priority queue will run next
	- The running process will be placed at the end of its priority queue
	- If no processes are available on ready queues, the INIT process will run

Any message saved to the PCB of the newly running process will be displayed. 
The message will then be cleared and deleted from the OS.

Reports:
	- The pre-empted process
	- The new running process PID and its priority
	- Any message saved to the PCB of the new running process

*** Messaging ***

Processes can send and receive messages, and reply to messages.
Sending a message:
	- Two possible scenarios:
		- The receiver is already blocked waiting to receive a message.
		The receiver will have the message copied to its PCB to display when it runs next.
		The receiver will be removed from the blocked queue and placed on a priority ready queue.
		- The receiver is not blocked waiting for a message.
		The message will be added to the OS inbox, which can later be retrieved if the process
		issues a receive command.
	- The sender will be placed on the blocked queue until a reply is received.
Any process can send to any process (including itself) and reply to any process.

*** Semaphores ***

- Five semaphores are provided for processes to use. They must be created with an ID from 0 to 4
and an initial value >= 0.
- Processes blocked on a semaphore after a P operation will be placed on the blocked queue and 
allow the next ready process to run.
- They will be woken up after a V operation and re-added to the appropriate ready queue.
- Each semaphore has its own list of processes blocked on itself.

*** Information ***

- Process information can be obtained through the Procinfo command.
- Information about all the priority queues and blocked queues can be obtained through the
Totalinfo command.