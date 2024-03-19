# Compiler
CC = gcc
CXX = g++

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11
CXXFLAGS = -Wall -Wextra -std=c++11

# Source files
SOURCES_C = parser.c
SOURCES_CPP = main.cpp

# Object files
OBJECTS_C = $(SOURCES_C:.c=.o)
OBJECTS_CPP = $(SOURCES_CPP:.cpp=.o)

# Executable name
EXECUTABLE = myprogram

# Main target
all: $(EXECUTABLE)

# Linking
$(EXECUTABLE): $(OBJECTS_C) $(OBJECTS_CPP)
	$(CXX) $(OBJECTS_C) $(OBJECTS_CPP) -o $@

# Compilation
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(OBJECTS_C) $(OBJECTS_CPP) $(EXECUTABLE)
