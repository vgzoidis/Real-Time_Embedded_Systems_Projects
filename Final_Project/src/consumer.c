#include "telemetry.h"
#include <string.h>
#include <stdio.h>
#include "cJSON.h" // We will use cJSON as requested

/*
 * Parses the JSON payload using cJSON as required by the assignment.
 */
static void parse_and_categorize(AppState *state, const char *json_payload) {
    // 1. Parse the JSON string
    cJSON *json = cJSON_Parse(json_payload);
    if (json == NULL) {
        // If parsing fails, count it as info/error and return safely
        pthread_mutex_lock(&state->counters_mutex);
        state->counters.info++;
        pthread_mutex_unlock(&state->counters_mutex);
        return;
    }

    uint32_t c_commit = 0, c_identity = 0, c_account = 0, c_info = 0;

    // 2. Extract the "kind" field
    cJSON *kind = cJSON_GetObjectItemCaseSensitive(json, "kind");
    if (cJSON_IsString(kind) && (kind->valuestring != NULL)) {
        if (strcmp(kind->valuestring, "commit") == 0) {
            c_commit = 1;
        } else if (strcmp(kind->valuestring, "identity") == 0) {
            c_identity = 1;
        } else if (strcmp(kind->valuestring, "account") == 0) {
            c_account = 1;
        } else {
            c_info = 1; // Other unrecognized kinds
        }
    } else {
        c_info = 1; // No "kind" field found
    }

    // 3. Atomically update global counters
    pthread_mutex_lock(&state->counters_mutex);
    state->counters.commit += c_commit;
    state->counters.identity += c_identity;
    state->counters.account += c_account;
    state->counters.info += c_info;
    pthread_mutex_unlock(&state->counters_mutex);

    // 4. CRITICAL: Free the cJSON object to prevent memory leaks!
    // Since cJSON uses malloc() under the hood to build its tree, we must free it.
    cJSON_Delete(json);
}

void* consumer_thread(void* arg) {
    AppState *state = (AppState*)arg;
    
    // Local buffer to hold the popped JSON payload (avoids keeping the mutex locked while parsing)
    char local_payload[MAX_PAYLOAD_SIZE];
    
    while (state->keep_running) {
        size_t payload_len = 0;
        
        // This will block (sleep) via pthread_cond_wait if the buffer is empty.
        // It wakes up instantly when the producer calls buffer_push().
        if (buffer_pop(&state->buffer, local_payload, &payload_len)) {
            // As per instructions: no printf() or file logging in this thread!
            // Just parse the message and bump the metrics.
            parse_and_categorize(state, local_payload);
        }
    }
    
    return NULL;
}
