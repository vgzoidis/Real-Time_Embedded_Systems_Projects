#include "telemetry.h"
#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>

#define FIREHOSE_URL "jetstream1.us-east.bsky.network"
#define FIREHOSE_PATH "/subscribe?wantedCollections=app.bsky.feed.post"

static AppState *global_app_state = NULL;

/* 
 * libwebsockets callback for handling WebSocket events 
 */
static int callback_firehose(struct lws *wsi, enum lws_callback_reasons reason,
                             void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("[Producer] Connected to Jetstream Firehose.\n");
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            // A new JSON frame arrived. Push it into our bounded circular buffer.
            if (global_app_state && len > 0) {
                // Determine if we have a full payload (we assume mostly complete text frames for simplicity)
                // In production lws, you might need to handle fragmented payloads.
                buffer_push(global_app_state, (const char *)in, len);
            }
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            printf("[Producer] Connection Error: %s\n", in ? (char *)in : "(null)");
            global_app_state->keep_running = false;
            break;

        case LWS_CALLBACK_CLOSED:
            printf("[Producer] Connection Closed.\n");
            global_app_state->keep_running = false;
            break;

        default:
            break;
    }
    return 0;
}

/* 
 * Define the supported protocols
 */
static struct lws_protocols protocols[] = {
    {
        "firehose-protocol",
        callback_firehose,
        0,
        MAX_PAYLOAD_SIZE,
    },
    { NULL, NULL, 0, 0 } /* terminator */
};

void* producer_thread(void* arg) {
    global_app_state = (AppState*)arg;
    
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    
    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "[Producer] Failed to create lws context\n");
        global_app_state->keep_running = false;
        return NULL;
    }
    
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = context;
    ccinfo.address = FIREHOSE_URL;
    ccinfo.port = 443;
    ccinfo.path = FIREHOSE_PATH;
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
    ccinfo.protocol = protocols[0].name;
    
    struct lws *wsi = lws_client_connect_via_info(&ccinfo);
    if (!wsi) {
        fprintf(stderr, "[Producer] Failed to connect to firehose\n");
        lws_context_destroy(context);
        global_app_state->keep_running = false;
        return NULL;
    }
    
    printf("[Producer] Starting event loop...\n");
    
    // Asynchronous lws event loop
    while (global_app_state->keep_running) {
        lws_service(context, 50); // wait up to 50ms for events
    }
    
    lws_context_destroy(context);
    printf("[Producer] Event loop terminated cleanly.\n");
    return NULL;
}
