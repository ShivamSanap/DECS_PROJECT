#pragma once

#include <iostream>
#include <string>
#include <optional>
#include <mutex>
#include <libpq-fe.h>

class DBConnector {
public:
    DBConnector(const std::string& conn_str) {
        m_conn = PQconnectdb(conn_str.c_str());
        if (PQstatus(m_conn) != CONNECTION_OK) {
            std::cerr << "Connection to database failed: " << PQerrorMessage(m_conn) << std::endl;
            PQfinish(m_conn);
            m_conn = nullptr;
        } else {
            std::cout << "Successfully connected to database." << std::endl;
        }
    }

    ~DBConnector() {
        if (m_conn) {
            PQfinish(m_conn);
        }
    }

    bool is_connected() const {
        return m_conn != nullptr;
    }

    bool put(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        const char* paramValues[2] = {key.c_str(), value.c_str()};
        const char* query = "INSERT INTO kv_pairs (key, value) VALUES ($1, $2) "
                            "ON CONFLICT (key) DO UPDATE SET value = $2;";
        
        PGresult* res = PQexecParams(m_conn, query, 2, NULL, paramValues, NULL, NULL, 0);
        
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::cerr << "PUT command failed: " << PQerrorMessage(m_conn) << std::endl;
            PQclear(res);
            return false;
        }
        
        PQclear(res);
        return true;
    }

    std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(m_mutex);

        const char* paramValues[1] = {key.c_str()};
        const char* query = "SELECT value FROM kv_pairs WHERE key = $1;";

        PGresult* res = PQexecParams(m_conn, query, 1, NULL, paramValues, NULL, NULL, 0);

        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::cerr << "GET command failed: " << PQerrorMessage(m_conn) << std::endl;
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

    bool remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        const char* paramValues[1] = {key.c_str()};
        const char* query = "DELETE FROM kv_pairs WHERE key = $1;";

        PGresult* res = PQexecParams(m_conn, query, 1, NULL, paramValues, NULL, NULL, 0);

        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::cerr << "DELETE command failed: " << PQerrorMessage(m_conn) << std::endl;
            PQclear(res);
            return false;
        }

        PQclear(res);
        return true;
    }

private:
    PGconn* m_conn;
    std::mutex m_mutex; 
};