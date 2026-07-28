# Define the compiler, allowing CROSS_COMPILE to prefix it
CC = $(CROSS_COMPILE)gcc

# Compiler flags
CFLAGS = -g -Wall
INCLUDES = -I/

# Default target
all: writer

# Link the object file to create the executable
writer: writer.o
	$(CC) $(CFLAGS) $(INCLUDES) writer.o -o writer

# Compile the C source file into an object file
writer.o: writer.c
	$(CC) $(CFLAGS) -c -o writer.o writer.c

# Clean target to remove the executable and object files
clean:
	rm -f writer writer.o

# Mark 'all' and 'clean' as phony targets (they are not files)
.PHONY: all clean