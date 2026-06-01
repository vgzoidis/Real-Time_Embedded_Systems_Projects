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

1. **Get the Toolchain:** Download the [AbhiTronix GCC 8.3.0 Raspberry Pi 1, Zero Toolchain](https://sourceforge.net/projects/raspberry-pi-cross-compilers/files/Raspberry%20Pi%20GCC%20Cross-Compiler%20Toolchains/Buster/GCC%208.3.0/Raspberry%20Pi%201%2C%20Zero/). Extract this somewhere accessible (e.g. `~/cross-pi-gcc-8.3.0-0` on WSL).
2. **Synchronize Sysroot:** The compiler needs to dynamically link `libwebsockets.so` and `pthread` meant for the Pi. You need to pull (`rsync`) the live `/usr` and `/lib` directories off the Pi to act as a "Sysroot".
   ```bash
   mkdir -p ~/pi-sysroot
   rsync -avz --rsync-path="sudo rsync" pi_user@192.168.X.X:/lib ~/pi-sysroot/
   rsync -avz --rsync-path="sudo rsync" pi_user@192.168.X.X:/usr ~/pi-sysroot/
   ```
3. **Compile:** Run the custom `pi` target to inject the ARM Architecture configurations and linker sysroot paths automatically. By default, the Makefile looks for the compiler at `/home/devcontainers/cross-pi-gcc-8.3.0-0/bin/arm-linux-gnueabihf-`. If your toolchain is located elsewhere, you can override it using `PI_CROSS_COMPILE`:
   ```bash
   make clean
   
   # If you are using the default devcontainer path:
   make pi
   
   # If you extracted the toolchain to a different path:
   make pi PI_CROSS_COMPILE=~/my-custom-path/bin/arm-linux-gnueabihf-
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

## 🛠️ Systemd Service (For Auto-Restarts and Robustness)

To run the telemetry system safely on boot and to ensure it automatically recovers from unexpected reboots, crashes, or power failures, use the provided systemd service. Since the Pi Zero is running headless, deploying this service is the safest way to ensure the 24-hour log succeeds.

1. **Adjust the Unit File for your User:**
   Before copying, ensure `telemetry.service` points to the correct paths and user for your specific Raspberry Pi setup. For example, if your username is `vgzoidis`:
   ```bash
   # In scripts/telemetry.service, adjust User, Group, ExecStart and WorkingDirectory:
   User=vgzoidis
   Group=vgzoidis
   ExecStart=/home/vgzoidis/Real-Time_Embedded_Systems_Projects/Final_Project/telemetry_logger
   WorkingDirectory=/home/vgzoidis/Real-Time_Embedded_Systems_Projects/Final_Project
   ```

2. **Deploy to the Pi (via SSH/SCP):**
   Copy the service file to your Pi:
   ```bash
   scp scripts/telemetry.service vgzoidis@192.168.X.X:~/
   ```

3. **Install and Enable on the Pi:**
   SSH into your Pi and move the file to the systemd directory:
   ```bash
   ssh vgzoidis@192.168.X.X
   sudo mv ~/telemetry.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable telemetry.service
   ```

4. **Start the Service:**
   ```bash
   sudo systemctl start telemetry.service
   ```

5. **Monitor & Manage:**
   - Check status: `sudo systemctl status telemetry.service`
   - Check logs: `sudo journalctl -u telemetry.service -f`
   - Stop service: `sudo systemctl stop telemetry.service`

## 🏃 Execution & Reconnection Testing

To run the telemetry logger on the Pi (headless):

Use `nohup` so the process outlives your SSH session and continues running in the background. Example:

```bash
# start in background and detach from the session
nohup ./telemetry_logger > telemetry_logger.out 2>&1 < /dev/null &
echo $! > telemetry_logger.pid
```

Notes:
- `telemetry_logger.out` contains stdout/stderr; inspect it with `tail -n 200 telemetry_logger.out`.
- To stop a background run started with `nohup` use the recorded PID:

```bash
kill -INT $(cat telemetry_logger.pid)
```

The application also supports an interactive shutdown (press `CTRL+C` when running foreground). The daemon handles `SIGINT`/`SIGTERM`, triggers an orderly teardown of the websocket loop, wakes waiting threads, flushes metrics, and exits cleanly.

### Self-Healing Reconnection Strategy
The application logic features a robust, self-healing producer thread. To test this on a headless device without losing your SSH session:
1. Start the application (`./telemetry_logger`).
2. Resolve the Firehose IP on the Pi:
   ```bash
   getent ahostsv4 jetstream1.us-east.bsky.network
   ```
3. Simulate a WAN-only outage (without dropping LAN/SSH) using a blackhole route:
   ```bash
   sudo ip route add blackhole <STREAM_IP>/32
   ```
   Do **not** execute `sudo ifconfig wlan0 down`, or you will instantly drop your SSH session.
4. Observe reconnect behavior from the log file (no terminal multiplexer needed):
   ```bash
   tail -n 120 telemetry_logger.out
   ```
5. Restore connectivity:
   ```bash
   sudo ip route del blackhole <JETSTREAM_IP>/32
   ```
   The application reconnects and continues without restart.

### Router-Unplug Outage Test (Full WiFi/Internet Loss)
You can also unplug the router to simulate a real outage. In this scenario, TCP sessions may not close instantly, so the producer now includes a receive-stall watchdog:
- If no frames are received for 12 seconds, the logger forces socket close and enters reconnect mode.
- Reconnect attempts run every 3 seconds until the link is back.

Headless-safe test flow:
1. Start logger with no-install background mode:
   ```bash
   nohup ./telemetry_logger > telemetry_logger.out 2>&1 < /dev/null &
   echo $! > telemetry_logger.pid
   ```
2. Unplug router WAN or power-cycle router.
3. After ~12 seconds of silence, expect log line similar to: `Receive stall detected (...)`.
4. Replug router and wait; expect reconnect attempts and then `Connected to Jetstream Firehose.` without restarting the process.

Quick runtime checks:
```bash
ls -l
ps aux | grep '[t]elemetry_logger'
ps -p $(cat telemetry_logger.pid) -o pid,etime,cmd
tail -n 100 telemetry_logger.out
```

#### Observed Validation (Real Pi Run)
In a successful headless test, `metrics_log.txt` showed normal traffic, then a forced outage window with zero processed events, followed by automatic recovery:
- Normal flow before outage: timestamps `1780166899` to `1780166922` had sustained non-zero event counts.
- Outage window: timestamps `1780166923` through `1780167017` were near-zero / zero event counts.
- Recovery after link restoration: from `1780167018` onward, events resumed (e.g., `10, 48, 37, 60, ...`), proving reconnection without process restart.

Corresponding producer log evidence from `telemetry_logger.out`:
- `Receive stall detected (18s). Forcing reconnect...`
- repeated reconnect attempts (`Attempting to reconnect...`, `Connection Error: Closed before conn`)
- eventual success: `Connected to Jetstream Firehose.`

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