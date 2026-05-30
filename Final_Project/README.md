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
# On Linux/WSL/Mac:
source venv/bin/activate
# On Windows PowerShell:
.\venv\Scripts\activate

pip install -r requirements.txt
```

## 🚀 Compilation & Build

### Standard Build (Host Native)
Navigate to the `Final_Project` directory and use the `make` utility.
```bash
cd Final_Project
make clean
make all
```
This will compile the `cJSON` library, the thread source codes, and link the `websockets` and `pthread` libraries, resulting in an executable named `telemetry_logger`.

### Cross-Compilation for Raspberry Pi Zero W (ARMv6)
A Raspberry Pi Zero W has limited RAM and processing power, making direct compilation of complex C projects slow and risky. It is a best practice to cross-compile the binary on your more powerful development PC.

1. **Get the Toolchain:** Download the [AbhiTronix GCC 8.3.0 Raspberry Pi 1, Zero Toolchain](https://sourceforge.net/projects/raspberry-pi-cross-compilers/files/Raspberry%20Pi%20GCC%20Cross-Compiler%20Toolchains/Buster/GCC%208.3.0/Raspberry%20Pi%201%2C%20Zero/). Extract this somewhere accessible (e.g. `~/cross-pi-gcc-8.3.0-0` on WSL) and ensure its `bin/` directory is in your `$PATH`.
2. **Synchronize Sysroot:** The compiler needs to dynamically link `libwebsockets.so` and `pthread` meant for the Pi. You need to pull (`rsync`) the live `/usr` and `/lib` directories off the Pi to act as a "Sysroot".
   ```bash
   mkdir -p ~/pi-sysroot
   rsync -avz --rsync-path="sudo rsync" pi_user@192.168.X.X:/lib ~/pi-sysroot/
   rsync -avz --rsync-path="sudo rsync" pi_user@192.168.X.X:/usr ~/pi-sysroot/
   ```
3. **Compile:** Run the custom `pi` target to inject the ARM Architecture configurations and linker sysroot paths automatically.
   ```bash
   make clean
   make pi
   ```
4. **Deploy:** Secure copy the compiled 32-bit `telemetry_logger` back to your Pi.
   ```bash
   scp telemetry_logger pi_user@192.168.X.X:~/
   ```
5. **Run:** Ssh into yout Pi and run the executable!
    ```bash
    ssh pi_user@192.168.X.X
    ./telemetry_logger
    ```

## 🏃 Execution & Reconnection Testing

To run the telemetry logger on the Pi:
```bash
./telemetry_logger
```
*Note: Due to the 24-hour operational requirement, you might want to run this inside a `tmux` or `screen` session to prevent unexpected terminal closures from killing the process.*

You can gracefully stop the process at any time by pressing `CTRL+C`. The daemon catches `SIGINT`, triggers a safe teardown of the websocket loop, frees the buffer contents, and exits the threads without corrupting memory.

### Self-Healing Reconnection Strategy
The application logic features a robust, self-healing producer thread. To test this on a headless device without losing your SSH session:
1. Start the application (`./telemetry_logger`).
2. Simulate a WAN network drop by unplugging your router's external internet cable, OR by temporarily dropping outbound packets to the Firehose via `iptables` (`sudo iptables -A OUTPUT -p tcp -d jetstream1.us-east.bsky.network -j DROP`). Do **not** execute `sudo ifconfig wlan0 down`, or you will instantly drop your SSH session!
3. Observe the console: The `libwebsockets` library will catch the connection error and cleanly close the socket. The outer C tracking loop will automatically assert a reconnect flag and attempt to re-dial the Firehose every 3 seconds.
4. Restore the internet connection (plug the cable back in, or execute `sudo iptables -D OUTPUT -p tcp -d jetstream1.us-east.bsky.network -j DROP`). The application will seamlessly recover, reconnect, and resume logging without needing a process restart.

## 📊 Analytics and Reporting

While running, the `Monitor` thread will write CSV-style data into `metrics_log.txt`:
`TIMESTAMP, CPU_UTILIZATION_PERCENT, PROCESSED_EVENTS_PER_SECOND`

### Fetching Data from the Raspberry Pi
Since running complex python graphical packages like `matplotlib` directly on the Raspberry Pi Zero W is incredibly slow, pull your logged data back to your PC!
Run this from your **Host PC** (e.g. WSL terminal):
```bash
# Pull the text file out of the Pi
scp pi_user@192.168.X.X:~/metrics_log.txt ./
```

### Generating Graphs
After transferring the log file back down, you can visualize the performance natively:
```bash
cd scripts
# On Linux/WSL/Mac:
source venv/bin/activate
# On Windows PowerShell:
.\venv\Scripts\activate

python analyze_metrics.py
```
This will read from `../metrics_log.txt` (or the folder it's in) and generate `performance_report.png` showcasing the CPU utilization versus Event Throughput over time.