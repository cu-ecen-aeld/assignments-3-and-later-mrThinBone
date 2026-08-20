# Define the compiler, allowing CROSS_COMPILE to prefix it
CC = $(CROSS_COMPILE)gcc

# Compiler flags
CFLAGS = -g -Wall -Werror
LDLIBS = -lpthread
INCLUDES = -I/

# Default target
all: aesdsocket

# Link the object file to create the executable
aesdsocket: server.o linkedlist.o
	$(CC) $(CFLAGS) $(INCLUDES) server.o linkedlist.o -o aesdsocket $(LDLIBS)

# Compile the C source file into an object file
linkedlist.o: linked_list.c
	$(CC) $(CFLAGS) -c -o linkedlist.o linked_list.c

server.o: server.c
	$(CC) $(CFLAGS) -c -o server.o server.c

# Clean target to remove the executable and object files
clean:
	rm -f aesdsocket server.o linkedlist.o

# Mark 'all' and 'clean' as phony targets (they are not files)
.PHONY: all clean