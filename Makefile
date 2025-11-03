# Compiler and flags
CXX = g++
CPPFLAGS = -I./include -I/usr/include/postgresql
CXXFLAGS = -std=c++17 -Wall -g

# --- Libraries ---
# Server needs PostgreSQL (libpq) and pthreads
SERVER_LDFLAGS = -lpq -lpthread
# Client (load_gen) only needs pthreads (for httplib)
CLIENT_LDFLAGS = -lpthread

# --- Targets ---

# 'all' rule builds both executables
all: server load_generator

# Rule to build the server
server: src/server.cpp include/db_connector.h include/lru_cache.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o server src/server.cpp $(SERVER_LDFLAGS)

# Rule to build the load generator
load_generator: src/load_generator.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o load_generator src/load_generator.cpp $(CLIENT_LDFLAGS)

# Clean rule
clean:
	rm -f server load_generator

