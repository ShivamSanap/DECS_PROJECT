#include <iostream>
#include <sstream> 
#include "../include/httplib.h"
#include "../include/lru_cache.h"
#include "../include/db_connector.h"

const std::string CONN_STR = "dbname=kvstore user=postgres password=shivam host=localhost port=5432";
const size_t CACHE_SIZE = 100; 

int main() {
    using namespace httplib;

    DBConnector db(CONN_STR);
    if (!db.is_connected()) {
        std::cerr << "Failed to start server: Could not connect to database." << std::endl;
        return 1;
    }

    LRUCache cache(CACHE_SIZE);

    Server svr;

    svr.Post("/create", [&](const Request& req, Response& res) {
        if (!req.has_param("key") || !req.has_param("value")) {
            res.set_content("Missing 'key' or 'value' parameters", "text/plain");
            res.status = 400; 
            return;
        }
        
        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");

        if (!db.put(key, value)) {
            res.set_content("Database operation failed", "text/plain");
            res.status = 500; 
            return;
        }

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

        auto cache_val = cache.get(key);
        if (cache_val) {
            res.set_content("Value (from cache): " + *cache_val, "text/plain");
            res.status = 200;
            return;
        }

        auto db_val = db.get(key);
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

        if (!db.remove(key)) {
            res.set_content("Database operation failed", "text/plain");
            res.status = 500; 
            return;
        }

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

    std::cout << "Server starting on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}