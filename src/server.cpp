#include <iostream>
#include <sstream> // ++ ADD THIS INCLUDE for string building ++
#include "../include/httplib.h"
#include "../include/lru_cache.h"
#include "../include/db_connector.h"

// --- Configuration ---
// !! IMPORTANT: Update with your PostgreSQL user and password !!
const std::string CONN_STR = "dbname=kvstore user=postgres password=shivam host=localhost port=5432";
const size_t CACHE_SIZE = 100; // Max number of items in cache
// ---------------------

int main() {
    using namespace httplib;

    // 1. Initialize Database Connector
    DBConnector db(CONN_STR);
    if (!db.is_connected()) {
        std::cerr << "Failed to start server: Could not connect to database." << std::endl;
        return 1;
    }

    // 2. Initialize LRU Cache
    LRUCache cache(CACHE_SIZE);

    // 3. Initialize HTTP Server
    Server svr;

    // --- Define API Endpoints ---

    // ... (keep the existing '/create', '/read', and '/delete' handlers as they are) ...

    /**
     * @brief Create: POST /create
     * Expects form data: "key" and "value"
     * Implements the "create" logic
     */
    svr.Post("/create", [&](const Request& req, Response& res) {
        if (!req.has_param("key") || !req.has_param("value")) {
            res.set_content("Missing 'key' or 'value' parameters", "text/plain");
            res.status = 400; // Bad Request
            return;
        }
        
        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");

        // 1. Store in database
        if (!db.put(key, value)) {
            res.set_content("Database operation failed", "text/plain");
            res.status = 500; // Internal Server Error
            return;
        }

        // 2. Store in cache
        cache.put(key, value);

        res.set_content("Successfully created/updated key: " + key, "text/plain");
        res.status = 200; // OK
    });

    /**
     * @brief Read: GET /read?key=...
     * Expects query parameter: "key"
     * Implements the "read" logic
     */
    svr.Get("/read", [&](const Request& req, Response& res) {
        if (!req.has_param("key")) {
            res.set_content("Missing 'key' parameter", "text/plain");
            res.status = 400; // Bad Request
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

        // 2. If miss, check database
        auto db_val = db.get(key);
        if (db_val) {
            // 3. Found in DB, insert into cache
            cache.put(key, *db_val);
            res.set_content("Value (from DB): " + *db_val, "text/plain");
            res.status = 200;
        } else {
            // 4. Not found in DB
            res.set_content("Key not found", "text/plain");
            res.status = 404; // Not Found
        }
    });

    /**
     * @brief Delete: DELETE /delete?key=...
     * Expects query parameter: "key"
     * Implements the "delete" logic
     */
    svr.Delete("/delete", [&](const Request& req, Response& res) {
        if (!req.has_param("key")) {
            res.set_content("Missing 'key' parameter", "text/plain");
            res.status = 400; // Bad Request
            return;
        }
        std::string key = req.get_param_value("key");

        // 1. Delete from database
        if (!db.remove(key)) {
            res.set_content("Database operation failed", "text/plain");
            res.status = 500; // Internal Server Error
            return;
        }

        // 2. Delete from cache (to maintain consistency)
        cache.remove(key);

        res.set_content("Successfully deleted key: " + key, "text/plain");
        res.status = 200;
    });


    // ++ ADD THIS NEW ENDPOINT ++
    /**
     * @brief Debug: GET /cache-status
     * Dumps the current state of the in-memory cache.
     */
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
    std::cout << "Server starting on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}