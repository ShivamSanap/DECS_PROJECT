#pragma once

#include <iostream>
#include <string>
#include <optional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <libpq-fe.h>

class DBConnectionPool {
public:
    DBConnectionPool(const std::string& conn_str, size_t pool_size) {
        for (size_t i = 0; i < pool_size; ++i) {
            PGconn* conn = PQconnectdb(conn_str.c_str());
            if (PQstatus(conn) != CONNECTION_OK) {
                std::cerr << "Failed to create connection " << i << ": " << PQerrorMessage(conn) << std::endl;
                PQfinish(conn);
            } else {
                m_pool.push(conn);
            }
        }
        if (m_pool.empty()) {
            std::cerr << "FATAL: No database connections could be established." << std::endl;
        } else {
            std::cout << "Successfully created connection pool with " << m_pool.size() << " connections." << std::endl;
        }
    }

    ~DBConnectionPool() {
        while (!m_pool.empty()) {
            PQfinish(m_pool.front());
            m_pool.pop();
        }
    }

    bool is_connected() const {
        return !m_pool.empty();
    }

    class PooledConnection {
    public:
        PooledConnection(DBConnectionPool& pool) : m_pool(pool), m_valid(true) {
            m_conn = m_pool.getConnection();
        }

        ~PooledConnection() {
            if (m_conn && m_valid) {
                m_pool.returnConnection(m_conn);
            }
        }

        PGconn* get() { return m_conn; }

        PooledConnection(const PooledConnection&) = delete;
        PooledConnection& operator=(const PooledConnection&) = delete;

        void invalidate() { m_valid = false; }

    private:
        DBConnectionPool& m_pool;
        PGconn* m_conn = nullptr;
        bool m_valid = false;
    };

    bool put(PGconn* conn, const std::string& key, const std::string& value) {
        if (!conn) return false;
        
        const char* paramValues[2] = {key.c_str(), value.c_str()};
        const char* query = "INSERT INTO kv_pairs (key, value) VALUES ($1, $2) "
                            "ON CONFLICT (key) DO UPDATE SET value = $2;";
        
        PGresult* res = PQexecParams(conn, query, 2, NULL, paramValues, NULL, NULL, 0);
        
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::cerr << "PUT command failed: " << PQerrorMessage(conn) << std::endl;
            PQclear(res);
            return false;
        }
        
        PQclear(res);
        return true;
    }

    std::optional<std::string> get(PGconn* conn, const std::string& key) {
        if (!conn) return std::nullopt;

        const char* paramValues[1] = {key.c_str()};
        const char* query = "SELECT value FROM kv_pairs WHERE key = $1;";

        PGresult* res = PQexecParams(conn, query, 1, NULL, paramValues, NULL, NULL, 0);

        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::cerr << "GET command failed: " << PQerrorMessage(conn) << std::endl;
            PQclear(res);
            return std::nullopt;
        }

        if (PQntuples(res) == 0) {
            PQclear(res);
            return std::nullopt;
        }

        std::string value = PQgetvalue(res, 0, 0);
        PQclear(res);
        return value;
    }

    bool remove(PGconn* conn, const std::string& key) {
        if (!conn) return false;
        
        const char* paramValues[1] = {key.c_str()};
        const char* query = "DELETE FROM kv_pairs WHERE key = $1;";

        PGresult* res = PQexecParams(conn, query, 1, NULL, paramValues, NULL, NULL, 0);

        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::cerr << "DELETE command failed: " << PQerrorMessage(conn) << std::endl;
            PQclear(res);
            return false;
        }

        PQclear(res);
        return true;
    }

private:

    PGconn* getConnection() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [this]{ return !m_pool.empty(); });
        
        PGconn* conn = m_pool.front();
        m_pool.pop();

        if (PQstatus(conn) != CONNECTION_OK) {
            std::cerr << "Connection lost. Attempting to reset..." << std::endl;
            PQreset(conn);
            if (PQstatus(conn) != CONNECTION_OK) {
                std::cerr << "Connection reset failed: " << PQerrorMessage(conn) << std::endl;
            }
        }

        return conn;
    }

    PGconn* getConnectionWithTimeout(int timeout_ms) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_cond.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]{ return !m_pool.empty(); })) {
            std::cerr << "Timeout waiting for database connection." << std::endl;
            return nullptr;
        }

        PGconn* conn = m_pool.front();
        m_pool.pop();

        if (PQstatus(conn) != CONNECTION_OK) {
            PQreset(conn);
            if (PQstatus(conn) != CONNECTION_OK) {
                std::cerr << "Connection reset failed: " << PQerrorMessage(conn) << std::endl;
            }
        }

        return conn;
    }

    void returnConnection(PGconn* conn) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pool.push(conn);
        m_cond.notify_one();
    }

    std::queue<PGconn*> m_pool;
    std::mutex m_mutex;
    std::condition_variable m_cond;
};