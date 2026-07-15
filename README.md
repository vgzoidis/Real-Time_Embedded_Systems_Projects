# Real-Time Embedded Systems Projects

This repository contains assignments and projects developed for the **Real-Time Embedded Systems** course. 

**Author:** Vasileios Zoidis (AEM: 10652)

## Repository Structure

### 📁 [Final_Project](./Final_Project)
Real-time telemetry logger for the Bluesky Jetstream Firehose, implemented in C.
* Uses a Producer-Consumer architecture with `libwebsockets`, `pthread`, and `cJSON`.
* Includes the source code, build files, Python analysis scripts, and the final report.
* Focuses on long-running execution, reconnect handling, and runtime metrics collection on resource-constrained hardware.

### 📁 [Project_1](./Project_1)
Implementation of the **Producer-Consumer** model using POSIX Threads (`pthreads`).
* Features a FIFO queue storing function pointers and arguments.
* Explores parallel programming concepts by measuring and minimizing the waiting time across multiple producer and consumer threads.
* Includes the complete source code (`prod-cons.c`), a PowerShell script for running experiments, and the final PDF report.

### 📁 [Paper_Presentation](./Paper_Presentation)
Slides and speaker notes for the scientific paper presentation. The presentation explores the critical role of Real-Time Operating Systems (RTOS) in supporting modern Cyber-Physical Systems (CPS). It is based on the IEEE paper:

> Serino, A., & Cheng, L. "Real-Time Operating Systems for Cyber-Physical Systems: Current Status and Future Research" (2020).

#### Key Topics Discussed
1. **The Nature of Cyber-Physical Systems (CPS):** Understanding the distinction between "Soft" and "Hard" real-time constraints and the catastrophic consequences of missed deadlines in hard real-time environments.
2. **RTOS vs. General Purpose OS (GPOS):** Why traditional operating systems fail in CPS scenarios. The benefits of priority-based scheduling versus fair time-sharing and of microkernel architectures over monolithic ones.
3. **The "Priority Inversion" Problem:** An analysis of deadlocks in shared resource allocation and the protocols used by modern RTOS to resolve them.
4. **Real-World Implementation:** A look at **JetOS**, an avionics-grade RTOS which utilizes strict partitioning to guarantee resource availability and safety.
5. **Future Research Directions:**
   - **Multi-core RTOS:** The shift towards asymmetric architectures to avoid resource locking overheads.
   - **Security:** Leveraging the inherent predictability and deterministic nature of RTOS execution times to detect anomalies and cyberattacks.

---

*Note: The code and report for **Project 0 (Parallel kNN Search)** can be found in its dedicated repository [here](https://github.com/vgzoidis/Parallel_kNN_Search).*
