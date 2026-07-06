// cpu_safe_multi_benchmark_no_temp.cpp
// Multi-tier CPU benchmark with temperature-related features removed.
// History file now contains only per-tier summary (no per-thread details).
// Compile (Windows / MinGW / Dev-C++):
// g++ "cpu_safe_multi_benchmark_no_temp.cpp" -O3 -march=native -ffast-math -pthread -std=c++11 -o cpu_benchmark.exe

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <string>
#include <ctime>
#include <fstream>

#ifdef _WIN32
  #include <windows.h>
  #define POPEN _popen
  #define PCLOSE _pclose
#else
  #include <unistd.h>
  #define POPEN popen
  #define PCLOSE pclose
#endif

// ---------- Configuration ----------
struct Tier { const char* name; int intensity; int duration_seconds; };
static const Tier TIERS[] = {
    {"EASY",    1,   15},
    {"MEDIUM",  2,   30},
    {"HARD",    4,   60},
    {"EXTREME", 8,  120}
};
static const unsigned long INNER_BASE = 200000u;
static const double FLOPS_PER_ITER = 12.0; // conservative ops/iteration estimate

// Safety thresholds (not used anymore but keep constants for clarity)
static const double THROTTLE_WARN_TEMP_C = 85.0;
static const double CRITICAL_TEMP_C      = 92.0;

// Monitor / timing
static const int TEMP_SAMPLE_INTERVAL_SEC = 1;
static const double THROTTLE_DETECT_RATIO = 0.70;
static const int COOLDOWN_SECONDS = 6;
static const int PAUSE_BETWEEN_TIERS = 6;
static const char HISTORY_FILENAME[] = "cpu_benchmark_history.txt";

// ---------- Shared state ----------
std::atomic<bool> running(false);
std::atomic<int> current_intensity(1);
std::atomic<unsigned long long> global_blocks(0ULL);
std::atomic<bool> abort_requested(false);
std::atomic<bool> aborted_by_monitor(false);
std::atomic<bool> aborted_by_signal(false);
std::mutex print_mutex;

// ---------- Utilities ----------
static FILE* safe_popen(const char* cmd, const char* mode) {
#ifdef _WIN32
    return _popen(cmd, mode);
#else
    return popen(cmd, mode);
#endif
}
static int safe_pclose(FILE* f) {
#ifdef _WIN32
    return _pclose(f);
#else
    return pclose(f);
#endif
}

static std::string now_string() {
    std::time_t t = std::time(nullptr);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[128];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf);
}

// trim
static std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Append a text block to history file (thread-safe)
static void append_history(const std::string &text) {
    std::lock_guard<std::mutex> lk(print_mutex);
    FILE* f = fopen(HISTORY_FILENAME, "a");
    if (!f) {
        std::cerr << "[WARN] couldn't open history file for writing: " << HISTORY_FILENAME << "\n";
        return;
    }
    fprintf(f, "%s\n", text.c_str());
    fclose(f);
}

// ---------- Signal handler ----------
static void handle_signal(int sig) {
    abort_requested.store(true);
    aborted_by_signal.store(true);
    running.store(false);
    std::lock_guard<std::mutex> lk(print_mutex);
    std::cerr << "\n[!] Signal " << sig << " received — aborting benchmark safely...\n";
}

// ---------- Worker and benchmark logic ----------
inline void worker_inner(volatile double &a, volatile double &b, volatile double &c, unsigned int iters) {
    for (unsigned int i=0;i<iters;++i) {
        a = a * 1.0000001 + b;
        b = b * 0.9999999 + a;
        c = a * b + c;
        a = a + b + c;
        b = b - a * 0.000001;
    }
}

struct ThreadCounters { std::atomic<unsigned long long> blocks{0ULL}; };

void worker_loop(int tid, ThreadCounters &tc) {
    volatile double a = 0.123456 + tid;
    volatile double b = 1.234567 + tid * 0.0001;
    volatile double c = 0.987654 + tid * 0.0002;
    while (running.load() && !abort_requested.load()) {
        int intensity = current_intensity.load();
        unsigned int iter_block = (unsigned int)(INNER_BASE * (unsigned long)intensity);
        worker_inner(a,b,c,iter_block);
        tc.blocks.fetch_add(1ULL, std::memory_order_relaxed);
        global_blocks.fetch_add(1ULL, std::memory_order_relaxed);
        if (!running.load() || abort_requested.load()) break;
    }
    if (a + b + c == 0.0) std::cout << "";
}

void run_tier_and_collect(const Tier &tier, unsigned int nthreads, std::ostringstream &out_report) {
    {
        std::lock_guard<std::mutex> lk(print_mutex);
        std::cout << "\n===== " << tier.name << " (" << tier.duration_seconds << "s, intensity x" << tier.intensity << ") =====\n";
    }

    std::vector<ThreadCounters> counters(nthreads);
    std::vector<std::thread> threads;
    global_blocks.store(0ULL);
    for (auto &c: counters) c.blocks.store(0ULL);
    current_intensity.store(tier.intensity);
    running.store(true);

    for (unsigned int i=0;i<nthreads;++i) threads.emplace_back(worker_loop, (int)i, std::ref(counters[i]));

    // baseline measurement (2s)
    auto t0 = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    unsigned long long blocks_after_baseline = global_blocks.load();
    auto t1 = std::chrono::high_resolution_clock::now();
    double baseline_sec = std::chrono::duration<double>(t1 - t0).count();
    double baseline_bps = (double)blocks_after_baseline / std::max(0.0001, baseline_sec);
    {
        std::lock_guard<std::mutex> lk(print_mutex);
        std::cout << "Baseline block rate: " << std::fixed << std::setprecision(2) << baseline_bps << " blocks/sec (approx)\n";
    }

    auto tier_start = std::chrono::high_resolution_clock::now();
    auto next_check = tier_start + std::chrono::seconds(3);
    auto tier_end = tier_start + std::chrono::seconds(tier.duration_seconds);

    double peak_ops_per_sec = 0.0;
    unsigned long long last_blocks = blocks_after_baseline;
    auto last_sample_time = t1;

    // Print live header (no temperature columns)
    {
        std::lock_guard<std::mutex> lk(print_mutex);
        std::cout << std::setw(8) << "Elapsed" << " | "
                  << std::setw(10) << "Blocks/s" << " | "
                  << std::setw(9) << "Gops/s" << " | "
                  << std::setw(9) << "Intensity" << "\n";
        std::cout << "-----------------------------------------------------------\n";
    }

    while (std::chrono::high_resolution_clock::now() < tier_end) {
        if (abort_requested.load()) break;
        // sample once/sec
        std::this_thread::sleep_for(std::chrono::seconds(1));

        unsigned long long blocks_now = global_blocks.load();
        auto now = std::chrono::high_resolution_clock::now();
        double sec_since_last = std::chrono::duration<double>(now - last_sample_time).count();
        if (sec_since_last <= 0.0) sec_since_last = 1.0;
        unsigned long long delta_blocks = blocks_now - last_blocks;

        int cur_int = current_intensity.load();
        double iter_per_block = (double)INNER_BASE * (double)cur_int;
        double blocks_per_sec = (double)delta_blocks / sec_since_last;
        double ops_sec = blocks_per_sec * iter_per_block * FLOPS_PER_ITER;
        if (ops_sec > peak_ops_per_sec) peak_ops_per_sec = ops_sec;

        last_blocks = blocks_now;
        last_sample_time = now;

        double elapsed = std::chrono::duration<double>(now - tier_start).count();

        // print aligned live line (no temps)
        {
            std::lock_guard<std::mutex> lk(print_mutex);
            std::cout << std::setw(7) << std::fixed << std::setprecision(1) << elapsed << "s | "
                      << std::setw(10) << std::fixed << std::setprecision(2) << blocks_per_sec << " | "
                      << std::setw(9) << std::fixed << std::setprecision(3) << (ops_sec/1e9) << " | "
                      << std::setw(9) << "x" << cur_int << "\n";
        }

        if (std::chrono::high_resolution_clock::now() >= next_check) {
            double elapsed_from_baseline = std::chrono::duration<double>(now - t1).count();
            if (elapsed_from_baseline < 0.5) elapsed_from_baseline = 0.5;
            double recent_bps = (double)(blocks_now - blocks_after_baseline) / elapsed_from_baseline;

            if (recent_bps < baseline_bps * THROTTLE_DETECT_RATIO) {
                {
                    std::lock_guard<std::mutex> lk(print_mutex);
                    std::cout << "[WARN] drop in block rate: " << std::fixed << std::setprecision(2) << recent_bps << " bps\n";
                }
                int cur = current_intensity.load();
                int new_int = std::max(1, cur/2);
                if (new_int < cur) current_intensity.store(new_int);
                {
                    std::lock_guard<std::mutex> lk(print_mutex);
                    std::cout << "Backing off intensity to x" << new_int << " and pausing " << COOLDOWN_SECONDS << "s\n";
                }
                std::this_thread::sleep_for(std::chrono::seconds(COOLDOWN_SECONDS));
                blocks_after_baseline = global_blocks.load();
                t1 = std::chrono::high_resolution_clock::now();
                baseline_bps = (double)blocks_after_baseline / std::max(0.0001, std::chrono::duration<double>(t1 - t0).count());
                last_blocks = blocks_after_baseline;
                last_sample_time = t1;
            } else {
                blocks_after_baseline = global_blocks.load();
                t1 = now;
            }
            next_check = std::chrono::high_resolution_clock::now() + std::chrono::seconds(3);
        }
    }

    running.store(false);
    for (auto &th : threads) if (th.joinable()) th.join();

    unsigned long long tier_blocks = 0ULL;
    for (unsigned int i=0;i<nthreads;++i) {
        unsigned long long b = counters[i].blocks.load();
        tier_blocks += b;
    }

    double actual_elapsed = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - tier_start).count();
    double approx_iters_per_block = (double)INNER_BASE * (double)tier.intensity;
    double total_ops = (double)tier_blocks * approx_iters_per_block * FLOPS_PER_ITER;

    double ops_billions = total_ops / 1e9;
    double avg_ops_per_sec_billions = (total_ops / actual_elapsed) / 1e9;
    double peak_ops_per_sec_billions = peak_ops_per_sec / 1e9;
    double gflops = (total_ops / actual_elapsed) / 1e9;

    {
        std::lock_guard<std::mutex> lk(print_mutex);
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "\n--- " << tier.name << " SUMMARY ---\n";
        std::cout << "Runtime (sec): " << actual_elapsed << "\n";
        std::cout << "Blocks completed: " << tier_blocks << "\n";
        std::cout << "Estimated operations: " << ops_billions << " billion ops\n";
        std::cout << "Average (billions/sec): " << avg_ops_per_sec_billions << "\n";
        std::cout << "Peak (billions/sec): " << peak_ops_per_sec_billions << "\n";
        std::cout << "Approx performance: " << gflops << " GFLOPS\n";
        std::cout << std::setprecision(3);
    }

    // prepare formatted block for history file: only summary, no per-thread details
    out_report << "----- " << tier.name << " -----\n";
    out_report << "Duration (s): " << actual_elapsed << "\n";
    out_report << "Blocks: " << tier_blocks << "\n";
    out_report << "Total ops (billion): " << ops_billions << "\n";
    out_report << "Average (billion/sec): " << avg_ops_per_sec_billions << "\n";
    out_report << "Peak (billion/sec): " << peak_ops_per_sec_billions << "\n";
    out_report << "GFLOPS: " << gflops << "\n";
    out_report << "\n";
}

std::vector<int> parse_selection(const std::string &s) {
    std::vector<int> out;
    std::string ss = trim(s);
    if (ss == "1") { out.push_back(0); return out; }
    if (ss == "1-2") { out.push_back(0); out.push_back(1); return out; }
    if (ss == "1-3") { out.push_back(0); out.push_back(1); out.push_back(2); return out; }
    if (ss == "1-4" || ss=="all") { out.push_back(0); out.push_back(1); out.push_back(2); out.push_back(3); return out; }
    std::istringstream iss(ss);
    std::string token;
    const int tiers_count = (int)(sizeof(TIERS)/sizeof(TIERS[0]));
    while (std::getline(iss, token, ',')) {
        token = trim(token);
        if (token.empty()) continue;
        try {
            int v = std::stoi(token);
            if (v >= 1 && v <= tiers_count) out.push_back(v-1);
        } catch(...) {}
    }
    return out;
}

int main(int argc, char** argv) {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
#ifdef SIGABRT
    std::signal(SIGABRT, handle_signal);
#endif
#ifdef SIGSEGV
    std::signal(SIGSEGV, handle_signal);
#endif

    unsigned int nthreads = std::thread::hardware_concurrency();
    if (nthreads == 0) nthreads = 1;

    {
        std::lock_guard<std::mutex> lk(print_mutex);
        std::cout << "===== SAFE MULTI-TIER CPU BENCHMARK =====\n";
        std::cout << "Logical threads: " << nthreads << "\n";
        std::cout << "Temperature monitoring features removed.\n\n";
    }

    {
        std::lock_guard<std::mutex> lk(print_mutex);
        std::cout << "\nChoose which tiers to run (type exactly):\n"
                  << "  1     -> EASY only\n"
                  << "  1-2   -> EASY + MEDIUM\n"
                  << "  1-3   -> EASY + MEDIUM + HARD\n"
                  << "  1-4   -> ALL tiers (EASY..EXTREME)\n"
                  << "Or enter comma-separated numbers (e.g. 1,3).\nYour choice: ";
    }

    std::string choice;
    if (!std::getline(std::cin, choice)) choice = "1-4";
    if (choice.empty()) choice = "1-4";
    auto sel = parse_selection(choice);
    if (sel.empty()) {
        std::lock_guard<std::mutex> lk(print_mutex);
        std::cout << "Invalid selection; defaulting to 1-4 (all tiers).\n";
        sel = parse_selection("1-4");
    }

    // Prepare history header
    std::ostringstream run_history;
    run_history << "============================\n";
    run_history << "Run timestamp: " << now_string() << "\n";
    run_history << "Threads: " << nthreads << "\n";
    run_history << "Configuration: INNER_BASE=" << INNER_BASE << " FLOPS_PER_ITER=" << FLOPS_PER_ITER << " (estimate)\n\n";

    // Run selected tiers and collect per-tier results into run_history
    for (int idx : sel) {
        if (abort_requested.load()) break;
        if (idx < 0 || idx >= (int)(sizeof(TIERS)/sizeof(TIERS[0]))) continue;
        run_tier_and_collect(TIERS[idx], nthreads, run_history);
    }

    // finalize
    abort_requested.store(true);
    running.store(false);

    // write history file with run_history contents
    append_history(run_history.str());

    // final status message
    {
        std::lock_guard<std::mutex> lk(print_mutex);
        if (aborted_by_signal.load()) {
            std::cout << "\n===== BENCHMARK ABORTED BY USER (signal) =====\n";
        } else if (aborted_by_monitor.load()) {
            std::cout << "\n===== BENCHMARK ABORTED DUE TO SAFETY (temperature) =====\n";
        } else {
            std::cout << "\n===== ALL SELECTED TIERS COMPLETE SUCCESSFULLY =====\n";
        }
        std::cout << "History appended to: " << HISTORY_FILENAME << "\n";
    }

    return 0;
}
