#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

/* Bounded buffer configuration */
#define BUFFER_CAPACITY 1024  // Must be power of 2 for fast wrapping, adjust based on Pi Zero memory
#define MAX_PAYLOAD_SIZE 4096 // Maximum expected JSON string size

/* Message types for logging */
typedef struct {
    uint32_t commit;
    uint32_t identity;
    uint32_t account;
    uint32_t info;
} MsgCounters;

/* Bounded Circular Queue for Raw JSON Strings */
typedef struct {
    // Pre-allocated 2D array to avoid malloc/free in the fast path
    char data[BUFFER_CAPACITY][MAX_PAYLOAD_SIZE];
    uint32_t head; // Write index (Producer)
    uint32_t tail; // Read index (Consumer)
    uint32_t count; // Current occupancy
    
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} CircularBuffer;

/* Global Application State */
typedef struct {
    CircularBuffer buffer;
    MsgCounters counters;
    pthread_mutex_t counters_mutex;
    
    volatile bool keep_running; // For graceful shutdown
} AppState;

/* CPU calculation state */
typedef struct {
    unsigned long long prev_total;
    unsigned long long prev_idle;
} CpuState;

/* Function Prototypes */
void init_app_state(AppState *state);
void destroy_app_state(AppState *state);

// Buffer operations
bool buffer_push(CircularBuffer *cb, const char *payload, size_t len);
bool buffer_pop(CircularBuffer *cb, char *output, size_t *out_len);
float get_buffer_occupancy_pct(CircularBuffer *cb);

// Thread entry points
void* producer_thread(void* arg);
void* consumer_thread(void* arg);
void* monitor_thread(void* arg);

#endif // TELEMETRY_H
