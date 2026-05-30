#include "telemetry.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

AppState app_state;

// Signal handler to gracefully stop the application (e.g., Ctrl+C)
void handle_sigint(int sig) {
    if (app_state.keep_running) {
        printf("\n[Main] Caught signal %d. Shutting down gracefully...\n", sig);
        app_state.keep_running = false;
        
        // Wake up resting threads so they can exit their loops
        pthread_mutex_lock(&app_state.buffer.mutex);
        pthread_cond_broadcast(&app_state.buffer.not_empty);
        pthread_cond_broadcast(&app_state.buffer.not_full);
        pthread_mutex_unlock(&app_state.buffer.mutex);
    }
}

int main() {
    printf("[Main] Initializing Real-Time Telemetry System...\n");
    
    // Register signal handler
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    
    // Initialize our global zero-allocation state
    init_app_state(&app_state);
    
    pthread_t prod_tid, cons_tid, mon_tid;
    
    // Start Threads in logical order
    // 1. Monitor (to start logging immediately)
    if (pthread_create(&mon_tid, NULL, monitor_thread, &app_state) != 0) {
        perror("Failed to create monitor thread");
        return 1;
    }
    
    // 2. Consumer (event-driven, will sleep until data arrives)
    if (pthread_create(&cons_tid, NULL, consumer_thread, &app_state) != 0) {
        perror("Failed to create consumer thread");
        return 1;
    }
    
    // 3. Producer (starts network connection and pumps data)
    if (pthread_create(&prod_tid, NULL, producer_thread, &app_state) != 0) {
        perror("Failed to create producer thread");
        return 1;
    }
    
    printf("[Main] All threads launched. System running. Press Ctrl+C to stop.\n");
    
    // Wait for threads to complete upon shutdown
    pthread_join(prod_tid, NULL);
    pthread_join(cons_tid, NULL);
    pthread_join(mon_tid, NULL);
    
    // Cleanup resources
    destroy_app_state(&app_state);
    printf("[Main] Shutdown complete. Resources freed.\n");
    
    return 0;
}
