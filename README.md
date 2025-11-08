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
