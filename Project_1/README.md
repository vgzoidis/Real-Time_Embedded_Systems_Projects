# Project 1: Producer-Consumer Problem Optimization

## Overview
This project focuses on solving the classic Producer-Consumer synchronization problem using POSIX threads (`pthreads`) in C. The goal is to extend a simple integer-based FIFO queue to handle complex tasks (function pointers and arguments) and to analyze the performance scalability by measuring and minimizing the average queue waiting time as the number of consumer threads changes.

## Assignment Requirements & Implementation
1. **Queue Payload Modification:** 
   The original integer FIFO queue was modified to store structures of type `struct workFunction`:
   ```c
   struct workFunction {
     void * (*work)(void *);
     void * arg;
   };
   ```
2. **Dynamic Workload:**
   * **Producers:** $p$ threads instantiate an array of 10 random angles and place `workFunction` structs into the queue, pointing to a custom `calculate_sine` function.
   * **Consumers:** $q$ threads run in an infinite loop (`while(1)`), dequeueing the structs and executing the associated function pointers.
3. **Removal of Artificial Delays:** 
   `sleep()` and `usleep()` calls were removed. Instead, a large number of iterations (`LOOP 10000`) was used to stress test the system.
4. **Precise Time Profiling:**
   Used `gettimeofday()` to measure the exact time an item spends *waiting in the queue* (from the moment it's enqueued by a producer to the moment it's dequeued by a consumer, *before* execution). To keep the required `workFunction` structure strictly unchanged, a parallel array (`timeBuf`) was added to the main `queue` structure to store timestamps.
5. **Systematic Experiments:**
   Tested various configurations (keeping $p=4$ constant and sweeping $q$ from $1$ to $128$) to find the number of consumer threads that minimizes the average waiting time.

## Files
* `prod-cons.c`: The core C source code containing the modified producer-consumer implementation.
* `run_experiments.ps1`: A PowerShell automation script that runs the C program across multiple configurations, performing 10 iterations per run to output stable, averaged execution statistics natively in a Markdown-friendly table.
* `report.tex`: A completely formatted LaTeX report detailing the methodology, hardware specifications, and experimental results, featuring a dynamically-generated `pgfplots` graph.

## How to Build and Run

### 1. Compile the C Program
Since the program uses POSIX threads and the Math library, compile it using `gcc` with the `-lpthread` and `-lm` flags:
```bash
gcc -o prod-cons prod-cons.c -lpthread -lm
```

### 2. Manual Execution
Run the compiled executable by specifying the number of producers and consumers:
```bash
# Usage: ./prod-cons <producers> <consumers>
./prod-cons 4 8
```

### 3. Automated Experiments
To generate the full table of wait times for various consumer thread counts, use the included PowerShell script (Windows):
```powershell
# You may need to bypass the execution policy first
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

.\run_experiments.ps1
```

## Summary of Findings
Testing on an AMD Ryzen 7 5800H (8 physical cores / 16 logical threads), the average waiting time decreased exponentially as consumer threads increased. Due to the extremely lightweight nature of the sine calculation workload, thread context switching overhead was minimal. The optimal minimum waiting time (approx. 8.66 μs) was reached at $q = 32$ consumers (with $p = 4$ producers). Pushing consumers significantly past this mark ($q \ge 64$) caused a slight increase in waiting times due to the OS overhead of managing excessive threads and mutex locks.