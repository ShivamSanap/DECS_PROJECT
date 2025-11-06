// ++ THIS IS THE FIX ++
// This macro MUST be defined *before* httplib.h is included.
// It tells httplib to create a thread pool with this many threads.
#define CPPHTTPLIB_THREAD_POOL_COUNT std::thread::hardware_concurrency()

#include <iostream>
#include <sstream>
#include "../include/httplib.h" // <-- Include comes AFTER the define
#include "../include/lru_cache.h"
#include "../include/db_connector.h" // This is our Pool header

// --- Configuration ---
const std::string CONN_STR = "dbname=kvstore user=postgres password=shivam host=localhost port=5432";
const size_t CACHE_SIZE = 100;
const size_t DB_POOL_SIZE = 16; // Size of our connection pool
// ---------------------

int main() {
    using namespace httplib;

    // 1. Initialize Database Connection Pool
    DBConnectionPool db_pool(CONN_STR, DB_POOL_SIZE);
    if (!db_pool.is_connected()) {
        std::cerr << "Failed to start server: Could not connect to database." << std::endl;
        return 1;
    }

    // 2. Initialize LRU Cache
    LRUCache cache(CACHE_SIZE);

    // 3. Initialize HTTP Server
    Server svr;
    
    // -- THE BAD LINE IS REMOVED --
    // The thread pool is now enabled by the #define at the top of the file.

    // --- Define API Endpoints ---

    svr.Post("/create", [&](const Request& req, Response& res) {
        if (!req.has_param("key") || !req.has_param("value")) {
            res.set_content("Missing 'key' or 'value' parameters", "text/plain");
            res.status = 400;
            return;
        }
        
        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");

        // Get a connection from the pool (this blocks if pool is empty)
        // It's auto-returned when 'conn' goes out of scope.
        DBConnectionPool::PooledConnection conn(db_pool);

        // 1. Store in database
        if (!db_pool.put(conn.get(), key, value)) {
            res.set_content("Database operation failed", "text/plain");
            res.status = 500;
            return;
        }

        // 2. Store in cache
        cache.put(key, value);

        res.set_content("Successfully created/updated key: " + key, "text/plain");
        res.status = 200;
    });

    svr.Get("/read", [&](const Request& req, Response& res) {
        if (!req.has_param("key")) {
            res.set_content("Missing 'key' parameter", "text/plain");
            res.status = 400;
            return;
        }
        std::string key = req.get_param_value("key");

        // 1. Check cache first
        auto cache_val = cache.get(key);
        if (cache_val) {
            res.set_content("Value (from cache): " + *cache_val, "text/plain");
            res.status = 200;
            return;
        }

        // Get a connection from the pool
        DBConnectionPool::PooledConnection conn(db_pool);

        // 2. If miss, check database
        auto db_val = db_pool.get(conn.get(), key);
        if (db_val) {
            cache.put(key, *db_val);
            res.set_content("Value (from DB): " + *db_val, "text/plain");
            res.status = 200;
        } else {
            res.set_content("Key not found", "text/plain");
            res.status = 404;
        }
    });

    svr.Delete("/delete", [&](const Request& req, Response& res) {
        if (!req.has_param("key")) {
            res.set_content("Missing 'key' parameter", "text/plain");
            res.status = 400;
            return;
        }
        std::string key = req.get_param_value("key");

        // Get a connection from the pool
        DBConnectionPool::PooledConnection conn(db_pool);

        // 1. Delete from database
        if (!db_pool.remove(conn.get(), key)) {
            res.set_content("Database operation failed", "text/plain");
            res.status = 500;
            return;
        }

        // 2. Delete from cache
        cache.remove(key);

        res.set_content("Successfully deleted key: " + key, "text/plain");
        res.status = 200;
    });

    svr.Get("/cache-status", [&](const Request& req, Response& res) {
        CacheState state = cache.get_state();
        std::stringstream ss;
        ss << "--- Cache Status ---\n";
        ss << "Occupied: " << state.current_size << " / " << state.max_size << "\n";
        ss << "\n--- Items (MRU to LRU) ---\n";
        if (state.items.empty()) {
            ss << "(Cache is empty)\n";
        } else {
            int count = 1;
            for (const auto& pair : state.items) {
                ss << count++ << ". Key: '" << pair.first << "', Value: '" << pair.second << "'\n";
            }
        }
        res.set_content(ss.str(), "text/plain");
        res.status = 200;
    });

    // --- Start the Server ---
    std::cout << "Server starting on port 8080 with connection pool..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}

