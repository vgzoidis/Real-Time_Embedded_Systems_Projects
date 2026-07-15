# Real-Time Telemetry Logger: Bluesky Jetstream Firehose

This project implements a highly efficient, real-time, multithreaded telemetry logging application in C. The application connects to the Bluesky Jetstream Firehose, which is a public, unauthenticated live WebSocket stream of network activity. Specifically, it filters for the `app.bsky.feed.post` collection to log system performance metrics alongside event ingestion rates. It is designed to run continuously for 24 hours on resource-constrained devices like the Raspberry Pi Zero W without memory leaks or clock drift.

## Architecture

The system follows a strict Producer-Consumer multithreading paradigm to ensure maximum efficiency:

*   **Producer Thread:** Uses `libwebsockets`, a lightweight pure C library designed for implementing modern network protocols with a tiny footprint. It asynchronously maintains an open WebSocket connection, collecting raw JSON bursts and pushing them into a shared thread-safe queue.
*   **Buffer (Queue):** A thread-safe bounded Circular Buffer (Fixed size of 1024 strings) protected by mutexes and condition variables. It prevents dynamic memory fragmentation by utilizing absolute pointer rotation.
*   **Consumer Thread:** Wakes up via `pthread_cond` signals. It fetches JSON payloads from the buffer, strictly parses them using `cJSON` to determine message types, and gracefully deallocates (`cJSON_Delete`) the AST structures to guarantee zero memory leaking over a 24-hour cycle.
*   **Monitor Thread:** A synchronous thread running at exactly 1Hz. By using `clock_nanosleep` with `TIMER_ABSTIME`, it eliminates cumulative clock drift. It polls `/proc/stat` to calculate CPU utilization, coordinates with the Producer/Consumer atomics to calculate event throughput, and writes the results to `metrics_log.txt`.

## Prerequisites & Setup

### System Dependencies

You need a Unix-like environment (e.g., Linux or Raspberry Pi OS) with development headers installed. Run the following commands:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake git
sudo apt-get install libwebsockets-dev
```

### Python Analytics Dependencies

Python is used for both pre-execution probing and post-execution analytics or graphing. It is highly recommended to use a virtual environment.

```bash
cd scripts
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

*(Note: On Windows PowerShell, use `.\venv\Scripts\activate` instead of `source`)*

## Compilation & Build

### Standard Build (Host Native)

Navigate to the `Final_Project` directory and use the `make` utility to compile for your current host architecture.

```bash
cd Final_Project
make clean
make all
```

This compiles the `cJSON` library and the thread source codes, linking the required `websockets` and `pthread` libraries. The resulting executable is named `telemetry_logger`.

### Cross-Compilation for Raspberry Pi Zero W (ARMv6)

A Raspberry Pi Zero W has limited RAM and processing power, making direct compilation of complex C projects slow. Standard practice dictates cross-compiling the binary on a more powerful development PC.

1.  **Get the Toolchain:** Download the AbhiTronix GCC 8.3.0 Raspberry Pi 1, Zero Toolchain and extract it to an accessible location (e.g., `~/cross-pi-gcc-8.3.0-0`).
2.  **Synchronize Sysroot:** Pull (`rsync`) the live `/usr` and `/lib` directories off the Pi to act as a "Sysroot", allowing the compiler to dynamically link the Pi's native `libwebsockets.so` and `pthread`.
3.  **Run Rsync commands:** Use `mkdir -p ~/pi-sysroot` followed by the necessary `rsync -avz --rsync-path="sudo rsync" pi_user@192.168.X.X:/lib ~/pi-sysroot/` and `rsync -avz --rsync-path="sudo rsync" pi_user@192.168.X.X:/usr ~/pi-sysroot/` commands.
4.  **Compile:** Run `make pi` to inject the ARM Architecture configurations and linker sysroot paths automatically. You can override the toolchain path using `make pi PI_CROSS_COMPILE=~/your-path/bin/arm-linux-gnueabihf-`.
5.  **Deploy:** Secure copy the compiled 32-bit `telemetry_logger` back to your Pi using `scp telemetry_logger pi_user@192.168.X.X:~/`.
6.  **Run:** SSH into your Pi and execute `./telemetry_logger`.

## Systemd Service (For Robustness)

To run the telemetry system safely on boot and ensure it automatically recovers from unexpected reboots, crashes, or power failures, use the provided systemd service.

1.  **Adjust the Unit File:** Edit `scripts/telemetry.service` to point to the correct user and directory paths for your specific setup (e.g., setting `User=vgzoidis` and mapping the `ExecStart` path).
2.  **Deploy to the Pi:** Transfer the service file using `scp scripts/telemetry.service user@192.168.X.X:~/`.
3.  **Install on the Pi:** Move the file to the systemd directory via `sudo mv ~/telemetry.service /etc/systemd/system/`.
4.  **Enable the Service:** Reload the daemon and enable it on boot using `sudo systemctl daemon-reload` followed by `sudo systemctl enable telemetry.service`.
5.  **Start and Monitor:** Use `sudo systemctl start telemetry.service` to run it, and `sudo journalctl -u telemetry.service -f` to monitor the logs.

## Execution & Reconnection Testing

For manual headless execution on the Pi, use `nohup` so the process outlives your SSH session.

```bash
nohup ./telemetry_logger > telemetry_logger.out 2>&1 < /dev/null &
echo $! > telemetry_logger.pid
```

To stop a background run cleanly, trigger an orderly teardown by using the recorded PID:

```bash
kill -INT $(cat telemetry_logger.pid)
```

### Self-Healing Reconnection Strategy

The application logic features a robust, self-healing producer thread. To test this on a headless device without losing your SSH session:

1.  **Start the logger:** Run the application manually or via the background script.
2.  **Resolve the IP:** Find the Firehose IP by running `getent ahostsv4 jetstream1.us-east.bsky.network`.
3.  **Simulate an outage:** Block WAN traffic without dropping your SSH LAN connection by adding a blackhole route: `sudo ip route add blackhole <STREAM_IP>/32`.
4.  **Observe the recovery:** Watch the reconnect attempts using `tail -n 120 telemetry_logger.out`.
5.  **Restore the link:** Remove the blackhole route using `sudo ip route del blackhole <STREAM_IP>/32` and watch the system automatically reconnect without restarting the process.

*(Note: Unplugging the router entirely will trigger the 12-second receive-stall watchdog, which safely forces a socket close and initiates the same 3-second redial backoff loop).*

## Analytics and Reporting

While running, the Monitor thread seamlessly writes CSV-style data into `metrics_log.txt` using the format: `TIMESTAMP, CPU_UTILIZATION_PERCENT, PROCESSED_EVENTS_PER_SECOND`.

### Generating Graphs

Because running complex Python graphical packages like `matplotlib` directly on the Raspberry Pi is inefficient, transfer the data back to your development PC.

```bash
scp pi_user@192.168.X.X:~/metrics_log.txt ./
```

After transferring the log file, activate your Python virtual environment and visualize the data.

```bash
python scripts/analyze_metrics.py
```

This produces `.png` charts and a `stats_summary.tex` file that summarize CPU utilization versus event throughput over time, buffer occupancy versus network burstiness, and the monitor thread's timing jitter.