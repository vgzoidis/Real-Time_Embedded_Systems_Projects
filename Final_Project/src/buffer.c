#include "telemetry.h"
#include <string.h>

void init_app_state(AppState *state) {
    memset(state, 0, sizeof(AppState));
    
    // Initialize mutexes and condition variables
    pthread_mutex_init(&state->buffer.mutex, NULL);
    pthread_cond_init(&state->buffer.not_empty, NULL);
    pthread_cond_init(&state->buffer.not_full, NULL);
    
    pthread_mutex_init(&state->counters_mutex, NULL);
    
    state->keep_running = true;
}

void destroy_app_state(AppState *state) {
    pthread_mutex_destroy(&state->buffer.mutex);
    pthread_cond_destroy(&state->buffer.not_empty);
    pthread_cond_destroy(&state->buffer.not_full);
    pthread_mutex_destroy(&state->counters_mutex);
}

bool buffer_push(CircularBuffer *cb, const char *payload, size_t len) {
    pthread_mutex_lock(&cb->mutex);
    
    // If buffer is full, we must drop the packet or wait.
    // In a real-time producer, dropping is sometimes preferred to blocking the network,
    // but the assignment states the producer wakes the consumer. We will block if full.
    while (cb->count == BUFFER_CAPACITY) {
        pthread_cond_wait(&cb->not_full, &cb->mutex);
    }
    
    // Copy data into the circular buffer
    size_t copy_len = len < (MAX_PAYLOAD_SIZE - 1) ? len : (MAX_PAYLOAD_SIZE - 1);
    memcpy(cb->data[cb->head], payload, copy_len);
    cb->data[cb->head][copy_len] = '\0'; // Null-terminate
    
    cb->head = (cb->head + 1) % BUFFER_CAPACITY;
    cb->count++;
    
    // Signal consumer that data is available
    pthread_cond_signal(&cb->not_empty);
    pthread_mutex_unlock(&cb->mutex);
    
    return true;
}

bool buffer_pop(CircularBuffer *cb, char *output, size_t *out_len) {
    pthread_mutex_lock(&cb->mutex);
    
    while (cb->count == 0) {
        pthread_cond_wait(&cb->not_empty, &cb->mutex);
    }
    
    // Copy data out
    size_t len = strlen(cb->data[cb->tail]);
    memcpy(output, cb->data[cb->tail], len + 1);
    if (out_len) *out_len = len;
    
    cb->tail = (cb->tail + 1) % BUFFER_CAPACITY;
    cb->count--;
    
    // Signal producer that space is available
    pthread_cond_signal(&cb->not_full);
    pthread_mutex_unlock(&cb->mutex);
    
    return true;
}

float get_buffer_occupancy_pct(CircularBuffer *cb) {
    pthread_mutex_lock(&cb->mutex);
    float pct = ((float)cb->count / (float)BUFFER_CAPACITY) * 100.0f;
    pthread_mutex_unlock(&cb->mutex);
    return pct;
}
