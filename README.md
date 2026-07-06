# Safe Multi-Tier CPU Benchmark

A compact but feature-rich **multi-tier CPU stress and throughput benchmark** written in C++. This program launches one worker thread per logical CPU core, runs configurable workload tiers, tracks approximate throughput, and writes a run summary to a history file.

This version is designed to be **simple to compile on Windows (MinGW / Dev-C++)** and also portable enough to build on other platforms with a standard C++ compiler.

---

## Table of Contents

- [Overview](#overview)
- [What This Benchmark Does](#what-this-benchmark-does)
- [Key Features](#key-features)
- [How It Works](#how-it-works)
- [Tier Definitions](#tier-definitions)
- [Performance Metrics](#performance-metrics)
- [Source Code Structure](#source-code-structure)
- [Build Instructions](#build-instructions)
  - [Windows / MinGW / Dev-C++](#windows--mingw--dev-c)
  - [Linux / macOS](#linux--macos)
- [Running the Program](#running-the-program)
- [Tier Selection Syntax](#tier-selection-syntax)
- [Output Explained](#output-explained)
- [History File](#history-file)
- [Signals and Safe Exit](#signals-and-safe-exit)
- [Tuning the Benchmark](#tuning-the-benchmark)
- [Interpreting Results](#interpreting-results)
- [Portability Notes](#portability-notes)
- [Known Limitations](#known-limitations)
- [Suggested Improvements](#suggested-improvements)
- [Troubleshooting](#troubleshooting)
- [License / Usage](#license--usage)

---

## Overview

This project is a **multi-tier CPU benchmark** that executes a floating-point heavy inner loop across multiple worker threads. It is intended to provide a repeatable, configurable workload that can be used to compare relative CPU performance under different load levels.

The program is organized into four preset tiers:

- **EASY**
- **MEDIUM**
- **HARD**
- **EXTREME**

Each tier changes the workload intensity and runtime. The benchmark prints live measurements while a tier is running and then emits a summary block at the end of each tier.

A run summary is appended to a text history file named:

```txt
cpu_benchmark_history.txt
```

---

## What This Benchmark Does

At a high level, the program:

1. Detects the number of logical CPU threads available.
2. Spawns one worker thread per logical thread.
3. Runs the selected benchmark tiers one by one.
4. Continuously performs floating-point math in a tight loop.
5. Samples approximate throughput during execution.
6. Prints a live status table for each tier.
7. Writes a per-tier summary to a history file.
8. Handles interrupt signals so the process can stop safely.

The benchmark is intentionally simple in its execution model so that the workload remains highly CPU-bound and predictable.

---

## Key Features

### Multi-tier execution
The benchmark supports multiple preset tiers with increasing intensity and duration.

### Per-logical-thread scaling
It uses `std::thread::hardware_concurrency()` to determine how many worker threads to launch.

### Live performance reporting
During each tier, the program prints:

- elapsed time
- blocks per second
- estimated GOPS/GFLOPS-style throughput
- current intensity

### Summary reporting
At the end of each tier, it prints:

- runtime
- blocks completed
- estimated total operations
- average throughput
- peak throughput
- approximate GFLOPS

### Persistent run history
The benchmark appends a run summary to a history file so results can be reviewed later.

### Portable thread and process helpers
The code includes platform-aware wrappers for `popen` / `pclose` naming consistency, even though the current version does not depend on external command execution.

### Safe termination support
The program installs signal handlers for common termination signals and stops the benchmark cleanly when interrupted.

---

## How It Works

The benchmark uses a simple workload function that repeatedly performs floating-point operations on three `volatile double` variables.

Each worker thread runs this inner loop repeatedly, and every completed loop iteration is counted as one **block**.

The program estimates total computational work using:

- `INNER_BASE`
- tier intensity
- `FLOPS_PER_ITER`

That means the throughput numbers are **estimates**, not hardware-verified measurements. They are useful for relative comparisons, but they should not be treated as exact scientific GFLOPS.

---

## Tier Definitions

The benchmark ships with four built-in tiers:

```cpp
static const Tier TIERS[] = {
    {"EASY",    1,   15},
    {"MEDIUM",  2,   30},
    {"HARD",    4,   60},
    {"EXTREME", 8,  120}
};
```

### EASY
- Intensity: `x1`
- Duration: `15 seconds`
- Best for a quick sanity check

### MEDIUM
- Intensity: `x2`
- Duration: `30 seconds`
- Useful for a medium-length workload sample

### HARD
- Intensity: `x4`
- Duration: `60 seconds`
- Good for extended comparison testing

### EXTREME
- Intensity: `x8`
- Duration: `120 seconds`
- Maximum built-in stress level in the current configuration

The **intensity** directly affects the number of inner-loop iterations per block.

---

## Performance Metrics

The program reports several metrics during and after each tier.

### Blocks
A **block** is one completed call to the inner worker function with the current iteration count.

### Blocks per second
This is a live approximation of how many blocks are completed each second across all worker threads.

### Operations estimate
The code estimates operations using:

```txt
ops = blocks × INNER_BASE × intensity × FLOPS_PER_ITER
```

### GOPS / GFLOPS-style throughput
The output shows throughput in billions of operations per second.

Important note: these are **estimated performance figures**, not a formal benchmark standard like SPEC or LINPACK.

### Peak throughput
The program tracks the highest observed throughput sample during each tier.

### Average throughput
The average throughput is computed from total estimated operations divided by runtime.

---

## Source Code Structure

The code is organized into several logical sections.

### 1. Includes and platform setup
The top section includes the C++ standard headers and platform-specific compatibility macros.

### 2. Configuration
This section defines the benchmark tiers, workload constants, thresholds, and timing values.

### 3. Shared state
Global atomics and mutexes coordinate between worker threads, the monitor logic, and the main thread.

### 4. Utility helpers
Helper functions handle:
- safe process opening/closing wrappers
- timestamp generation
- string trimming
- history file writing

### 5. Signal handler
A signal handler sets abort flags and logs a message when termination signals arrive.

### 6. Worker logic
Each worker thread repeatedly executes the floating-point inner loop.

### 7. Tier execution
This function runs one benchmark tier, samples performance, prints live output, and writes the tier summary.

### 8. Tier selection parser
The user’s input is parsed into a list of tier indices.

### 9. Program entry point
The `main()` function initializes signals, prompts the user, runs the selected tiers, and appends results to the history file.

---

## Build Instructions

## Windows / MinGW / Dev-C++

### MinGW command line
Open a terminal in the source directory and compile with:

```bash
g++ -std=c++17 -O2 -pthread benchmark.cpp -o benchmark.exe
```

If your MinGW environment requires it, you may also use:

```bash
g++ -std=c++17 -O2 benchmark.cpp -o benchmark.exe
```

### Dev-C++
1. Create a new console application project.
2. Paste the code into `main.cpp`.
3. Ensure the project uses at least **C++17** mode if available.
4. Build and run the project.

### Notes for Windows users
- The code uses `_popen` / `_pclose` macros for compatibility.
- `std::thread` requires threading support from your compiler and runtime.
- On some toolchains, you may need to link against `-static -static-libgcc -static-libstdc++` for portable executables.

---

## Linux / macOS

Compile with a modern C++ compiler:

```bash
g++ -std=c++17 -O2 -pthread benchmark.cpp -o benchmark
```

Run it with:

```bash
./benchmark
```

On Unix-like systems, the code uses `popen` / `pclose`.

---

## Running the Program

After launching the executable, the program will:

1. Display the number of logical threads detected.
2. Ask you which benchmark tiers to run.
3. Execute the selected tiers one after another.
4. Print live throughput lines during each tier.
5. Print a summary at the end of each tier.
6. Append the final run report to the history file.

Example startup flow:

```txt
===== SAFE MULTI-TIER CPU BENCHMARK =====
Logical threads: 12
Temperature monitoring features removed.

Choose which tiers to run (type exactly):
  1     -> EASY only
  1-2   -> EASY + MEDIUM
  1-3   -> EASY + MEDIUM + HARD
  1-4   -> ALL tiers (EASY..EXTREME)
Or enter comma-separated numbers (e.g. 1,3).
Your choice:
```

---

## Tier Selection Syntax

The program accepts several input forms.

### Predefined shortcuts
- `1` → run EASY only
- `1-2` → run EASY and MEDIUM
- `1-3` → run EASY, MEDIUM, and HARD
- `1-4` or `all` → run all tiers

### Comma-separated selection
You can also enter custom combinations:

```txt
1,3
```

That would run EASY and HARD.

### Invalid input handling
If the input cannot be parsed or no valid tiers are selected, the program defaults to all tiers.

---

## Output Explained

During each tier, the benchmark prints a live table.

Example format:

```txt
Elapsed |   Blocks/s |    Gops/s | Intensity
-----------------------------------------------------------
    3.0s |    102.54 |     2.461 |       x1
    4.0s |    101.11 |     2.426 |       x1
```

### Columns

#### Elapsed
Time since the current tier started.

#### Blocks/s
Approximate number of completed blocks per second.

#### Gops/s
Estimated billions of operations per second.

#### Intensity
The current tier intensity multiplier.

---

## History File

At the end of a run, the program appends a text summary to:

```txt
cpu_benchmark_history.txt
```

Each run adds a block similar to:

```txt
============================
Run timestamp: 2026-07-06 12:34:56
Threads: 12
Configuration: INNER_BASE=200000 FLOPS_PER_ITER=12 (estimate)

----- EASY -----
Duration (s): 15.002
Blocks: 812
Total ops (billion): 1.949
Average (billion/sec): 0.130
Peak (billion/sec): 0.161
GFLOPS: 0.130

----- MEDIUM -----
...
```

### Important history policy
The history file now stores **only per-tier summaries**. It does **not** store per-thread details.

This keeps the file compact and easier to review over time.

### File behavior
- The file is opened in append mode
- Existing history is preserved
- Each run creates a new block separated by header lines

---

## Signals and Safe Exit

The program installs handlers for several signals:

- `SIGINT`
- `SIGTERM`
- `SIGABRT`
- `SIGSEGV` when available

When a signal is received:

1. The program sets abort flags.
2. Worker loops stop naturally.
3. Threads are joined.
4. A status message is printed.
5. Any collected summary is appended to the history file.

This is intended to avoid abrupt termination where possible.

---

## Tuning the Benchmark

Several constants control benchmark behavior.

### `INNER_BASE`
Controls the base number of inner iterations per block.

- Higher values increase workload per block.
- Lower values reduce runtime load.

### `FLOPS_PER_ITER`
Controls the operations estimate used in reporting.

- It is an approximation, not a hardware measurement.
- Adjust it only if you want a different accounting model.

### Tier intensity
Each tier multiplies the inner iteration count.

Example:
- EASY = `x1`
- MEDIUM = `x2`
- HARD = `x4`
- EXTREME = `x8`

### Tier duration
The runtime for each tier is fixed in the `TIERS` array.

### Cooldown and monitoring values
The current code still contains a few constants related to throttling logic and cooldown timing, even though temperature monitoring has been removed from the live reporting path.

---

## Interpreting Results

This benchmark is best used for **relative comparisons** rather than absolute claims.

### Good use cases
- comparing performance before and after a hardware change
- comparing compiler builds or optimization settings
- comparing runs across different machines
- tracking whether a system is behaving consistently over time

### What the numbers mean
- Higher blocks/sec usually means better throughput.
- Higher estimated GFLOPS usually means the CPU completed more floating-point work per second.
- Peak throughput may reflect short bursts rather than sustained performance.

### What can affect results
- CPU model
- number of logical threads
- thermal limits
- power plan / governor settings
- background processes
- compiler optimization level
- operating system scheduling
- memory pressure

For cleaner comparisons, close other heavy applications before running the benchmark.

---

## Portability Notes

Although the example is written with Windows in mind, it includes conditional compilation blocks for cross-platform support.

### Windows-specific items
- `_popen` / `_pclose`
- `localtime_s`
- `windows.h`

### Non-Windows items
- `popen` / `pclose`
- `localtime_r`
- `unistd.h`

### Compiler requirements
A C++17-capable compiler is recommended.

### Thread support
The benchmark depends heavily on `std::thread`, so the runtime must support POSIX threads on Unix-like systems or native threading on Windows.

---

## Known Limitations

This project is intentionally lightweight, which means it has some limitations.

### 1. Estimated FLOPS, not hardware counters
The benchmark uses a simple formula to estimate performance.
It does not read CPU performance counters or use vendor-specific APIs.

### 2. No per-core pinning
Threads are not pinned to specific cores.
The operating system scheduler decides placement.

### 3. No temperature display in the current version
The comments indicate that temperature-related features were removed.
The current UI and history output do not report temperature.

### 4. No power measurement
The program does not measure wattage or energy efficiency.

### 5. No statistical confidence intervals
The output shows raw sampled values without deeper statistical analysis.

### 6. Global workload model only
All threads run the same inner loop and do not simulate mixed workloads.

---

## Suggested Improvements

Here are several practical enhancements you could add later.

### 1. JSON output
Add an optional JSON export for easier automated parsing.

### 2. CSV export
A CSV mode would make historical comparison in spreadsheets much easier.

### 3. Per-core affinity
Pin worker threads to fixed logical cores for more consistent results.

### 4. Real thermal monitoring
If desired, integrate platform-specific temperature APIs or external sensor reads.

### 5. CPU feature detection
Display CPU model, architecture, and instruction set support at startup.

### 6. Better statistics
Track min / max / median sample throughput per tier.

### 7. Progress estimation
Show a percentage completed for each tier.

### 8. Optional CLI arguments
Allow tier selection and duration overrides directly from command line parameters.

### 9. Benchmark profiles
Support presets such as `quick`, `balanced`, `stress`, and `overnight`.

### 10. Automatic environment capture
Log OS version, compiler version, and CPU topology into the history file.

---

## Troubleshooting

### The program immediately exits
Check whether the input selection was valid. If the selection is invalid, the program may default to the full tier list.

### The executable will not compile
Make sure:
- you are using a C++17-capable compiler
- threading support is enabled
- the source file is saved with a `.cpp` extension

### `std::thread::hardware_concurrency()` returns 0
The code already falls back to `1` thread if detection fails.

### Performance numbers look unstable
That is expected in a simple benchmark. Try:
- closing background programs
- running the benchmark multiple times
- using the same power plan each time
- avoiding laptop battery mode

### History file is not created
The program appends to the file in the current working directory.
If the file cannot be opened:
- verify write permissions
- check whether the working directory is writable
- confirm that the process is not blocked by security software

### Output is hard to read
Run the benchmark from a standard terminal window with enough width to display aligned columns properly.

---

## License / Usage

No license was specified in the source code. Before redistributing or publishing the project, decide whether you want:

- permissive reuse
- internal-only use
- open-source publication
- a custom license header

If you plan to share the code publicly, it is a good idea to add a clear license file and a short project description at the top of the repository.

---

## Example Repository Layout

A simple project layout could look like this:

```txt
cpu-benchmark/
├── main.cpp
├── README.md
├── cpu_benchmark_history.txt
└── LICENSE
```

---

## Final Notes

This benchmark is intentionally focused on **repeatable CPU load generation**, **lightweight reporting**, and **simple result logging**.

It is a good fit for:

- personal benchmarking
- quick performance comparisons
- classroom demonstrations
- build validation under CPU load
- smoke testing a new machine or compiler setup

For best results, run multiple times under similar conditions and compare the trend rather than relying on a single number.

