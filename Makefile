CC = gcc
CFLAGS = -g -Wall -Wextra -I -pthread
PROG = proc
OBJS = process.o list.o

proc: $(OBJS)
	$(CC) $(CFLAGS) -o $(PROG) $(OBJS)

list.o: list.c
	$(CC) $(CFLAGS) -c list.c

proc.o: proc.c list.h
	$(CC) $(CFLAGS) -c main.c

clean:
	rm *.o proc