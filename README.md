<h1 align="center">HTTP Based Key-Value Server with Caching</h1>

<p>
This project implements an HTTP-based key-value server developed in C++. The system is designed to efficiently store, retrieve, and manage key-value pairs through simple HTTP requests. It combines three main components:

    1. A lightweight HTTP server built using the cpp-httplib library.
    2. An in-memory Least Recently Used (LRU) cache for fast data access.
    3. A PostgreSQL database that provides persistent storage for key-value data.

The overall goal of this system is to offer a simple, scalable, and consistent key-value store that can handle HTTP requests directly while minimizing latency through intelligent caching and ensuring data durability through a robust database backend.
</p>

![Architecture Diagram](architecture.png)

## Setup Instructions:

1. Install PostgreSQL
```bash
sudo apt update
sudo apt install postgresql postgresql-contrib
```

2. Install libpqxx (PostgreSQL C++ connector)
```bash
sudo apt install libpqxx-dev
```

3. Install httplib (HTTP library for C++)
If you are using the header-only version:
```bash
wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
```

## API Endpoints Testing:

1. Insert into Database:
```bash
curl -X POST -d "key=<your_key>&value=<your_value>" http://localhost:8080/create
```

2. Read from Database:
```bash
curl -X GET http://localhost:8080/read?key=<your_key>
```
This command will check if your key exists in cache first, if yes return the value from cache or return the value from the database.

3. Delete from Database:
```bash
curl -X DELETE http://localhost:8080/delete?key=<your_key>
```
This command will also delete the key value pair from the cache (if exists).

4. Check Cache Status:
```bash
curl http://localhost:8080/cache-status
```

- Shivam Sharad Sanap gate 2025