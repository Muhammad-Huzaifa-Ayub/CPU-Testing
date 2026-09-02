#include <iostream>
#include <thread>
#include <vector>
#include <cmath>
#include <chrono>
#include <atomic>
#include <iomanip>

std::atomic<bool> running(true);

// Each worker keeps track of how many loops it completes
void stress_worker(int id, std::atomic<unsigned long long>& counter)
{
    volatile double x = 0.000001;
    volatile double y = 1.234567;

    while (running.load())
    {
        // Heavy math block
        for (int i = 0; i < 1000000; ++i)
        {
            x = std::sin(x) * std::cos(y) + std::sqrt(std::fabs(x + y));
            y = std::tan(x) + std::log(y + 1.0);
        }
        counter++; // record completed workload block
    }
}

int main()
{
    unsigned int threads = std::thread::hardware_concurrency();

    std::cout << "===== SCIENTIFIC CPU STRESS TEST =====\n";
    std::cout << "Logical threads detected: " << threads << "\n";

    int duration_seconds = 300; // change duration here

    std::vector<std::thread> workers;
    std::vector<std::atomic<unsigned long long>> counters(threads);

    for (unsigned int i = 0; i < threads; ++i)
        counters[i] = 0;

    auto start_time = std::chrono::high_resolution_clock::now();

    std::cout << "Starting workload for " << duration_seconds << " seconds...\n";

    // Launch threads
    for (unsigned int i = 0; i < threads; ++i)
        workers.emplace_back(stress_worker, i, std::ref(counters[i]));

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));

    running.store(false);

    for (auto &t : workers)
        t.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed =
        std::chrono::duration<double>(end_time - start_time).count();

    // ===== SUMMARY CALCULATION =====
    unsigned long long total_blocks = 0;

    for (unsigned int i = 0; i < threads; ++i)
        total_blocks += counters[i].load();

    // Each block � 1,000,000 loop iterations
    // Rough estimate: ~10 floating ops per iteration
    double total_operations = (double)total_blocks * 1000000.0 * 10.0;

    double gflops = (total_operations / elapsed) / 1e9;

    // ===== RESULT OUTPUT =====
    std::cout << "\n===== RESULT SUMMARY =====\n";
    std::cout << "Total runtime: " << std::fixed << std::setprecision(2)
              << elapsed << " seconds\n";

    std::cout << "Total workload blocks: " << total_blocks << "\n";
    std::cout << "Estimated operations: " << std::scientific
              << total_operations << "\n";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Approx performance: " << gflops << " GFLOPS\n";

    std::cout << "\nPer-thread results:\n";
    for (unsigned int i = 0; i < threads; ++i)
    {
        std::cout << "Thread " << i << ": "
                  << counters[i].load() << " blocks\n";
    }

    std::cout << "===== TEST COMPLETE =====\n";
    return 0;
}
