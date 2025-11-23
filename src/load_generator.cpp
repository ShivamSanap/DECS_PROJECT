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

std::atomic<bool> g_time_to_stop{false};       
std::atomic<long long> g_successful_requests{0};
std::atomic<long long> g_failed_requests{0};
std::mutex g_metrics_mutex;
double g_total_response_time_ms = 0.0;      

const int POPULAR_KEY_RANGE = 100;  
const int PUT_DELETE_PERCENT = 10; 

bool key_exists(httplib::Client& cli, const std::string& key) {
    auto res = cli.Get("/read?key=" + key);
    return res && (res->status == 200);
}

void warm_up_popular_keys(const std::string& server_addr) {
    std::cout << "Starting automatic cache warm-up for get_popular workload..." << std::endl;

    httplib::Client cli(server_addr);
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(5, 0);

    bool need_warmup = false;
    for (int i = 1; i <= 5; ++i) { 
        if (!key_exists(cli, "key_" + std::to_string(i))) {
            need_warmup = true;
            break;
        }
    }

    if (!need_warmup) {
        std::cout << "Popular keys already exist in DB. Skipping warm-up." << std::endl;
        return;
    }

    for (int i = 1; i <= POPULAR_KEY_RANGE; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = "val_" + std::to_string(i);
        httplib::Params params{{"key", key}, {"value", value}};
        auto res = cli.Post("/create", params);

        if (!res || res->status != 200) {
            std::cerr << "Warm-up failed for " << key;
            if (res) std::cerr << " (HTTP " << res->status << ")";
            std::cerr << std::endl;
        }
    }

    std::cout << "Cache warm-up complete. 100 keys populated." << std::endl;
}

void client_thread(int thread_id, std::string server_addr, std::string workload_type) {
    long long local_successful_requests = 0;
    long long local_failed_requests = 0;
    double local_total_response_time_ms = 0.0;
    long long request_counter = 0;

    httplib::Client cli(server_addr);
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(5, 0);

    std::mt19937 rng(std::random_device{}() + thread_id);
    std::uniform_int_distribution<> popular_key_dist(1, POPULAR_KEY_RANGE);
    std::uniform_int_distribution<> mix_dist(1, 100);

    while (!g_time_to_stop) {
        std::string key, value;
        httplib::Result res;
        request_counter++;

        auto start_time = std::chrono::high_resolution_clock::now();

        try {
            if (workload_type == "get_popular") {
                key = "key_" + std::to_string(popular_key_dist(rng));
                res = cli.Get("/read?key=" + key);

            }  else if (workload_type == "put_all") {
                key = "put_all_" + std::to_string(thread_id) + "_" + std::to_string(request_counter);
                if (mix_dist(rng) <= PUT_DELETE_PERCENT) {
                    res = cli.Delete("/delete?key=" + key);
                } else {
                    value = std::string(4096, 'x'); 
                    httplib::Params params{{"key", key}, {"value", value}};
                    res = cli.Post("/create", params);
                }

            } 
        } catch (const std::exception& e) {
            std::cerr << "Thread " << thread_id << " exception: " << e.what() << std::endl;
            local_failed_requests++;
            continue;
        }

        auto end_time = std::chrono::high_resolution_clock::now();

        if (res && (res->status == 200 || res->status == 404)) {
            local_successful_requests++;
            std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
            local_total_response_time_ms += elapsed.count();
        } else {
            local_failed_requests++;
            if (!res) {
                std::cerr << "Thread " << thread_id << " request failed: " << httplib::to_string(res.error()) << std::endl;
            } else {
                std::cerr << "Thread " << thread_id << " server error: " << res->status << std::endl;
            }
        }
    }

    g_successful_requests += local_successful_requests;
    g_failed_requests += local_failed_requests;

    std::lock_guard<std::mutex> lock(g_metrics_mutex);
    g_total_response_time_ms += local_total_response_time_ms;
}

void print_usage() {
    std::cerr << "Usage: ./load_generator <server_addr> <num_threads> <duration_sec> <workload_type>\n"
              << "Example: ./load_generator http://127.0.0.1:8080 16 30 get_popular\n"
              << "Valid workload_types:\n"
              << "  get_popular : (Cache-Hit) Reads popular keys (auto warm-up)\n"
              << "  put_all     : (Write I/O) 90% PUTs, 10% DELETEs\n";
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        print_usage();
        return 1;
    }

    std::string server_addr = argv[1];
    int num_threads = std::stoi(argv[2]);
    int duration_sec = std::stoi(argv[3]);
    std::string workload_type = argv[4];

    if (num_threads <= 0 || duration_sec <= 0) {
        std::cerr << "Threads and duration must be > 0.\n";
        return 1;
    }
    if (workload_type != "get_popular" && workload_type != "put_all") {
        std::cerr << "Invalid workload type.\n";
        print_usage();
        return 1;
    }

    std::cout << "--- CS744 Load Generator ---\n"
              << "Target Server:   " << server_addr << "\n"
              << "Load Level:      " << num_threads << " threads\n"
              << "Test Duration:   " << duration_sec << " seconds\n"
              << "Workload:        " << workload_type << "\n"
              << "-----------------------------\n";

    if (workload_type == "get_popular") {
        warm_up_popular_keys(server_addr);
    }

    std::cout << "Starting load test..." << std::endl;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
        threads.emplace_back(client_thread, i, server_addr, workload_type);

    std::this_thread::sleep_for(std::chrono::seconds(duration_sec));

    g_time_to_stop = true;
    std::cout << "\nStopping threads..." << std::endl;
    for (auto& t : threads) t.join();

    double avg_throughput = g_successful_requests.load() / (double)duration_sec;
    double avg_response_time = (g_successful_requests.load() == 0)
                                   ? 0
                                   : (g_total_response_time_ms / g_successful_requests.load());

    std::cout << "\n--- Load Test Results ---\n"
              << "Total Successful Requests: " << g_successful_requests.load() << "\n"
              << "Total Failed Requests:     " << g_failed_requests.load() << "\n"
              << "Test Duration:             " << duration_sec << " s\n\n"
              << "Average Throughput:      " << avg_throughput << " reqs/sec\n"
              << "Average Response Time:   " << avg_response_time << " ms\n"
              << "---------------------------\n";

    return 0;
}