# Makefile

# Compiler and flags
CC=gcc
CFLAGS=-fdiagnostics-color=always -g -Wall

# Source files
SOURCES=compiler.c hw_config.c inst_stack.c

# Object files
OBJECTS=$(SOURCES:.c=.o)

# Target executable
TARGET=compiler

# Default target
all: $(TARGET)

# Linking the target executable from the object files
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

# # Compiling source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Cleaning up the compilation products
clean:
	rm -f $(OBJECTS) $(TARGET)
