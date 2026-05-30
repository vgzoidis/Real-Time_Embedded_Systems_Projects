# Real-Time Telemetry Logger: Bluesky Jetstream Firehose

This project implements a highly efficient, real-time, multithreaded telemetry logging application in C. The application connects to the Bluesky Jetstream Firehose (websocket), specifically filtering for the `app.bsky.feed.post` collection, and logs system performance metrics alongside event ingestion rates. It is designed to run continuously for 24 hours on resource-constrained devices like the Raspberry Pi Zero W without memory leaks or clock drift.

## 🏗️ Architecture

The system follows a strict **Producer-Consumer** multithreading paradigm:
- **Producer Thread:** Uses `libwebsockets` to asynchronously maintain an open WebSocket connection, collecting raw JSON bursts and pushing them into a shared thread-safe queue.
- **Buffer (Queue):** A thread-safe bounded Circular Buffer (Fixed size of 1024 strings) protected by mutexes and condition variables. Prevents dynamic memory fragmentation by utilizing absolute pointer rotation.
- **Consumer Thread:** Wakes up via `pthread_cond` signals. Fetches JSON payloads from the buffer, strictly parses them using `cJSON` to determine message types, and gracefully deallocates (`cJSON_Delete`) the AST structures to guarantee zero memory leaking over a 24-hour cycle.
- **Monitor Thread:** A synchronous thread running at exactly 1Hz (using `clock_nanosleep` with `TIMER_ABSTIME` to eliminate cumulative clock drift). It polls `/proc/stat` to calculate CPU utilization and coordinates with the Producer/Consumer atomics to calculate event consumption throughput, writing to `metrics_log.txt`.

## 🛠️ Prerequisites & Setup

### System Dependencies
You need a Unix-like environment (e.g., Linux/Raspberry Pi OS) with development headers.
```bash
sudo apt-get update
sudo apt-get install build-essential cmake git
sudo apt-get install libwebsockets-dev
```

### Python Analytics Dependencies
Python is used for both pre-execution probing and post-execution analytics/graphing.
It is highly recommended to use a virtual environment.
```bash
cd scripts
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

## 🚀 Compilation & Build

Navigate to the `Final_Project` directory and use the `make` utility.
```bash
cd Final_Project
make clean
make all
```
This will compile the `cJSON` library, the thread source codes, and link the `websockets` and `pthread` libraries, resulting in an executable named `telemetry_logger`.

## 🏃 Execution

To run the telemetry logger:
```bash
./telemetry_logger
```
*Note: Due to the 24-hour operational requirement, you might want to run this inside a `tmux` or `screen` session to prevent unexpected terminal closures from killing the process.*

You can gracefully stop the process at any time by pressing `CTRL+C`. The daemon catches `SIGINT`, triggers a safe teardown of the websocket loop, frees the buffer contents, and exits the threads without corrupting memory.

## 📊 Analytics and Reporting

While running, the `Monitor` thread will write CSV-style data into `metrics_log.txt`:
`TIMESTAMP, CPU_UTILIZATION_PERCENT, PROCESSED_EVENTS_PER_SECOND`

After terminating the logger, you can generate the analytical graphs:
```bash
cd scripts
source venv/bin/activate
python analyze_metrics.py
```
This will read from `../metrics_log.txt` and generate `performance_report.png` showcasing the CPU utilization versus Event Throughput over time.