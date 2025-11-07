<h1 align="center">HTTP Based Key-Value Server with Caching</h1>

<p>
This project implements an HTTP-based key-value server developed in C++. The system is designed to efficiently store, retrieve, and manage key-value pairs through simple HTTP requests. It combines three main components:

    1. A lightweight HTTP server built using the cpp-httplib library.
    2. An in-memory Least Recently Used (LRU) cache for fast data access.
    3. A PostgreSQL database that provides persistent storage for key-value data.

The overall goal of this system is to offer a simple, scalable, and consistent key-value store that can handle HTTP requests directly while minimizing latency through intelligent caching and ensuring data durability through a robust database backend.
</p>

![Architecture Diagram](architecture.png)

Setup Instructions

Before running the server, make sure you have all required dependencies installed.

1. Install PostgreSQL
sudo apt update
sudo apt install postgresql postgresql-contrib

2. Install libpqxx (PostgreSQL C++ connector)
sudo apt install libpqxx-dev

3. Install httplib (HTTP library for C++)

If you are using the header-only version:

wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h


Place the httplib.h file inside your project directory.

Steps to Execute
1. Compile the Server
make

2. Start the Server
./server


Initially, the in-memory cache will be empty.

3. Database Commands

Once PostgreSQL is running and your database is set up, you can manually inspect or modify the table:

SELECT * FROM kv_pairs;
DELETE FROM kv_pairs;

API Endpoints (Testing with curl)

You can interact with the key-value store using simple curl commands.

Insert into Database

curl -X POST -d "key=<your_key>&value=<your_value>" http://localhost:8080/create


Read from Database and Cache

curl -X GET http://localhost:8080/read?key=<key_value>


The server checks the cache first:

If the key is found in cache → returns value directly from cache.

Otherwise → fetches from database, caches it, and then returns it.

Check Cache Status

curl http://localhost:8080/cache-status


Delete from Database (and Cache if exists)

curl -X DELETE http://localhost:8080/delete?key=hello