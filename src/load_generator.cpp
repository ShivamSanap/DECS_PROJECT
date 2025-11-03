#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <random>
#include <sstream>
#include "../include/httplib.h"

// --- Global Shared State ---

// Atomic flag to signal worker threads to stop
std::atomic<bool> g_time_to_stop{false};

// Global counters for metrics
std::atomic<long long> g_successful_requests{0};
std::atomic<long long> g_failed_requests{0};
std::mutex g_metrics_mutex;
double g_total_response_time_ms = 0.0; // Protected by g_metrics_mutex

// --- Configuration for Workloads ---
const int POPULAR_KEY_RANGE = 100; // For 'get_popular': keys "key_1" to "key_100"
const int MIX_GET_PERCENT = 80;   // For 'get_put_mix': 80% GETs (popular), 20% PUTs (unique)
const int PUT_DELETE_PERCENT = 10;  // For 'put_all': 90% PUTs, 10% DELETEs

/**
 * @brief Generates a random alphanumeric string of a given length.
 */
std::string generate_random_string(std::mt19937& rng, int length) {
    std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<> dist(0, chars.length() - 1);
    std::string result = "";
    for (int i = 0; i < length; ++i) {
        result += chars[dist(rng)];
    }
    return result;
}

/**
 * @brief The main function for each client (worker) thread.
 * * @param thread_id A unique ID for this thread (0 to num_threads-1).
 * @param server_addr The full server address (e.g., "http://127.0.0.1:8080").
 * @param workload_type The name of the workload to run.
 */
void client_thread(int thread_id, std::string server_addr, std::string workload_type) {
    // --- Per-thread state ---
    long long local_successful_requests = 0;
    long long local_failed_requests = 0;
    double local_total_response_time_ms = 0.0;
    long long request_counter = 0; // For generating unique keys

    // Each thread gets its own HTTP client
    httplib::Client cli(server_addr);
    cli.set_connection_timeout(2, 0); // 2-second connection timeout
    cli.set_read_timeout(5, 0);       // 5-second read timeout

    // Each thread gets its own random number generator
    // Seeded with a mix of random_device and thread_id for uniqueness
    std::mt19937 rng(std::random_device{}() + thread_id);
    std::uniform_int_distribution<> popular_key_dist(1, POPULAR_KEY_RANGE);
    std::uniform_int_distribution<> mix_dist(1, 100);
    
    // --- Closed-Loop Load Generation ---
    // Runs until the main thread sets g_time_to_stop to true
    while (!g_time_to_stop) {
        std::string key;
        std::string value;
        httplib::Result res;
        request_counter++;

        // Start timing
        auto start_time = std::chrono::high_resolution_clock::now();

        // 1. Generate Request based on workload
        try {
            if (workload_type == "get_popular") {
                key = "key_" + std::to_string(popular_key_dist(rng));
                res = cli.Get("/read?key=" + key);

            } else if (workload_type == "get_all") {
                // Generate unique keys to guarantee cache misses
                key = "get_all_" + std::to_string(thread_id) + "_" + std::to_string(request_counter);
                res = cli.Get("/read?key=" + key);

            } else if (workload_type == "put_all") {
                // 90% PUT, 10% DELETE
                key = "put_all_" + std::to_string(thread_id) + "_" + std::to_string(request_counter);
                if (mix_dist(rng) <= PUT_DELETE_PERCENT) {
                    // DELETE
                    res = cli.Delete("/delete?key=" + key);
                } else {
                    // PUT (create)
                    value = generate_random_string(rng, 64);
                    httplib::Params params{{"key", key}, {"value", value}};
                    res = cli.Post("/create", params);
                }

            } else { // "get_put_mix"
                if (mix_dist(rng) <= MIX_GET_PERCENT) {
                    // 80% GET (popular)
                    key = "key_" + std::to_string(popular_key_dist(rng));
                    res = cli.Get("/read?key=" + key);
                } else {
                    // 20% PUT (unique)
                    key = "mix_put_" + std::to_string(thread_id) + "_" + std::to_string(request_counter);
                    value = generate_random_string(rng, 64);
                    httplib::Params params{{"key", key}, {"value", value}};
                    res = cli.Post("/create", params);
                }
            }
        } catch (const std::exception& e) {
            // Catch any exceptions during request generation (should be rare)
            std::cerr << "Thread " << thread_id << " exception: " << e.what() << std::endl;
            local_failed_requests++;
            continue; // Go to next request
        }

        // Stop timing
        auto end_time = std::chrono::high_resolution_clock::now();

        // 2. Record Metrics
        if (res && (res->status == 200 || res->status == 404)) {
            // Count 200 (OK) and 404 (Not Found) as "successful" operations
            local_successful_requests++;
            std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
            local_total_response_time_ms += elapsed.count();
        } else {
            local_failed_requests++;
            // Log errors for debugging
            if (!res) {
                 // This means a connection failure, timeout, etc.
                 std::cerr << "Thread " << thread_id << " request failed: " << httplib::to_string(res.error()) << std::endl;
            } else {
                 // This means a server error (e.g., 500, 400)
                 std::cerr << "Thread " << thread_id << " server error: " << res->status << std::endl;
            }
        }
    }

    // --- Test is over, report local metrics to global counters ---
    g_successful_requests += local_successful_requests;
    g_failed_requests += local_failed_requests;

    std::lock_guard<std::mutex> lock(g_metrics_mutex);
    g_total_response_time_ms += local_total_response_time_ms;
}

/**
 * @brief Prints the command-line usage instructions.
 */
void print_usage() {
    std::cerr << "Usage: ./load_generator <server_addr> <num_threads> <duration_sec> <workload_type>\n"
              << "Example: ./load_generator http://127.0.0.1:8080 16 30 get_popular\n"
              << "Valid workload_types: \n"
              << "  get_popular : (Cache-Hit) Reads from a small, popular set of keys.\n"
              << "  get_all     : (Cache-Miss) Reads with unique keys to miss cache.\n"
              << "  put_all     : (DB-Write) 90% PUTs (unique) and 10% DELETEs (unique).\n"
              << "  get_put_mix : 80% GETs (popular) and 20% PUTs (unique).\n";
}

int main(int argc, char* argv[]) {
    // 1. Parse Command-Line Arguments
    if (argc != 5) {
        print_usage();
        return 1;
    }

    std::string server_addr;
    int num_threads;
    int duration_sec;
    std::string workload_type;

    try {
        server_addr = argv[1];
        num_threads = std::stoi(argv[2]);
        duration_sec = std::stoi(argv[3]);
        workload_type = argv[4];

        if (num_threads <= 0 || duration_sec <= 0) {
            throw std::invalid_argument("Threads and duration must be > 0.");
        }
        if (workload_type != "get_popular" && workload_type != "get_all" &&
            workload_type != "put_all" && workload_type != "get_put_mix") {
            throw std::invalid_argument("Invalid workload_type.");
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing arguments: " << e.what() << "\n\n";
        print_usage();
        return 1;
    }

    // 2. Print Test Configuration
    std::cout << "--- CS744 Load Generator ---\n"
              << "Target Server:   " << server_addr << "\n"
              << "Load Level:      " << num_threads << " threads\n"
              << "Test Duration:   " << duration_sec << " seconds\n"
              << "Workload:        " << workload_type << "\n"
              << "-----------------------------\n"
              << "Starting load test..." << std::endl;

    // 3. Launch Worker Threads
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(client_thread, i, server_addr, workload_type);
    }

    // 4. Wait for the test duration
    std::this_thread::sleep_for(std::chrono::seconds(duration_sec));

    // 5. Signal threads to stop and wait for them
    g_time_to_stop = true;
    std::cout << "\nTime's up! Signaling threads to stop and collecting results..." << std::endl;
    for (auto& t : threads) {
        t.join();
    }
    std::cout << "All threads finished." << std::endl;

    // 6. Compute and Print Final Metrics
    double avg_throughput = g_successful_requests.load() / (double)duration_sec;
    double avg_response_time = (g_successful_requests.load() == 0) ? 
                               0 : (g_total_response_time_ms / g_successful_requests.load());

    std::cout << "\n--- Load Test Results ---\n"
              << "Total Successful Requests: " << g_successful_requests.load() << "\n"
              << "Total Failed Requests:     " << g_failed_requests.load() << "\n"
              << "Test Duration:             " << duration_sec << " s\n"
              << "\n"
              << "Average Throughput:      " << avg_throughput << " reqs/sec\n"
              << "Average Response Time:   " << avg_response_time << " ms\n"
              << "---------------------------\n";

    return 0;
}
