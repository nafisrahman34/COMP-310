framesize = 69
varmemsize = 100
CC=gcc
CFLAGS= 


myshell: shell.c interpreter.c shellmemory.c
	$(CC) $(CFLAGS) -D FRAMESIZE=$(framesize) -D VARMEMSIZE=$(varmemsize) -c -g shell.c interpreter.c shellmemory.c kernel.c pcb.c ready_queue.c
	$(CC) $(CFLAGS) -o myshell shell.o interpreter.o shellmemory.o kernel.o pcb.o ready_queue.o

clean: 
	rm myshell; rm *.o
