# Compiler and flags
CXX = g++
CPPFLAGS = -I./include -I/usr/include/postgresql
CXXFLAGS = -std=c++17 -Wall -g
LDFLAGS = -lpq -lpthread

# Target
TARGET = server

# Source files
SOURCES = src/server.cpp

# Default build rule
all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

# Clean rule
clean:
	rm -f $(TARGET)