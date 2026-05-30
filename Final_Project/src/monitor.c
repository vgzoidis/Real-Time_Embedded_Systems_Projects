#include "telemetry.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

/* Helper to read CPU stats from /proc/stat */
static void read_cpu_state(unsigned long long *total, unsigned long long *idle) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return;

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fp)) {
        char cpu_label[16];
        unsigned long long user, nice, system, idl, iowait, irq, softirq, steal, guest, guest_nice;
        
        sscanf(buffer, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
               cpu_label, &user, &nice, &system, &idl, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);

        *idle = idl + iowait;
        *total = user + nice + system + idl + iowait + irq + softirq + steal + guest + guest_nice;
    }
    fclose(fp);
}

void* monitor_thread(void* arg) {
    AppState *state = (AppState*)arg;
    
    FILE *fp = fopen("metrics_log.txt", "a");
    if (!fp) {
        perror("Failed to open metrics_log.txt");
        return NULL;
    }
    
    // Print CSV header if the file is empty/new
    fseek(fp, 0, SEEK_END);
    if (ftell(fp) == 0) {
        fprintf(fp, "Seconds,Nanoseconds,Commit_Count,Identity_Count,Account_Count,Info_Count,Buffer_Occupancy_Pct,CPU_Pct\n");
        fflush(fp);
    }

    CpuState cpu_state = {0};
    read_cpu_state(&cpu_state.prev_total, &cpu_state.prev_idle);

    struct timespec next_wake_time;
    clock_gettime(CLOCK_REALTIME, &next_wake_time);

    while (state->keep_running) {
        // Strict absolute time progression (+1 second)
        next_wake_time.tv_sec += 1;
        
        // This stops clock drift entirely over 24h
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_wake_time, NULL);
        
        // Get precise timestamp right after waking up to record true Jitter
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);

        // 1. Calculate CPU % overhead
        unsigned long long cur_total = 0, cur_idle = 0;
        read_cpu_state(&cur_total, &cur_idle);
        
        unsigned long long diff_total = cur_total - cpu_state.prev_total;
        unsigned long long diff_idle = cur_idle - cpu_state.prev_idle;
        
        float cpu_pct = 0.0f;
        if (diff_total > 0) {
            cpu_pct = (float)(diff_total - diff_idle) / diff_total * 100.0f;
        }
        
        cpu_state.prev_total = cur_total;
        cpu_state.prev_idle = cur_idle;

        // 2. Safely read and reset counters
        pthread_mutex_lock(&state->counters_mutex);
        uint32_t snapshot_commit = state->counters.commit;
        uint32_t snapshot_identity = state->counters.identity;
        uint32_t snapshot_account = state->counters.account;
        uint32_t snapshot_info = state->counters.info;
        
        state->counters.commit = 0;
        state->counters.identity = 0;
        state->counters.account = 0;
        state->counters.info = 0;
        pthread_mutex_unlock(&state->counters_mutex);

        // 3. Get buffer occupancy
        float buf_pct = get_buffer_occupancy_pct(&state->buffer);

        // 4. Log to CSV
        fprintf(fp, "%ld,%ld,%u,%u,%u,%u,%.2f,%.2f\n",
                ts.tv_sec, ts.tv_nsec, 
                snapshot_commit, snapshot_identity, snapshot_account, snapshot_info, 
                buf_pct, cpu_pct);
        fflush(fp); // Ensure it writes to SD card continuously
    }

    fclose(fp);
    return NULL;
}
