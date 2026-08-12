# Define the compiler, allowing CROSS_COMPILE to prefix it
CC = $(CROSS_COMPILE)gcc

# Compiler flags
CFLAGS = -g -Wall -Werror
INCLUDES = -I/

# Default target
all: aesdsocket

# Link the object file to create the executable
aesdsocket: server.o
	$(CC) $(CFLAGS) $(INCLUDES) server.o -o aesdsocket

# Compile the C source file into an object file
server.o: server.c
	$(CC) $(CFLAGS) -c -o server.o server.c

# Clean target to remove the executable and object files
clean:
	rm -f aesdsocket server.o

# Mark 'all' and 'clean' as phony targets (they are not files)
.PHONY: all clean