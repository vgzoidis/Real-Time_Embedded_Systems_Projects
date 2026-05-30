import asyncio
import websockets
import json
import time
from collections import Counter

URI = "wss://jetstream1.us-east.bsky.network/subscribe?wantedCollections=app.bsky.feed.post"

async def probe_firehose():
    print(f"Connecting to Jetstream Firehose:\n{URI}")
    
    try:
        async with websockets.connect(URI) as websocket:
            print("Connected! Listening for messages...\n")
            
            counters = Counter()
            start_time = time.time()
            message_count = 0
            seen_kinds = set()
            
            while True:
                message_str = await websocket.recv()
                message_count += 1
                
                try:
                    data = json.loads(message_str)
                    kind = data.get("kind", "unknown")
                    counters[kind] += 1
                    
                    # Print the first message of each kind to inspect its structure
                    if kind not in seen_kinds:
                        seen_kinds.add(kind)
                        print(f"\n[{kind.upper()}] Sample Message Structure:")
                        print(json.dumps(data, indent=2))
                        print("-" * 50)
                        
                except json.JSONDecodeError:
                    print("Failed to parse JSON")
                
                # Every 1 second, print the stats (Hz and Distribution)
                current_time = time.time()
                elapsed = current_time - start_time
                if elapsed >= 1.0:
                    hz = message_count / elapsed
                    print(f"[METRICS] Rate: {hz:.2f} Hz | Types/sec: {dict(counters)}")
                    
                    # Reset window for next second
                    start_time = current_time
                    message_count = 0
                    counters.clear()
                    
    except KeyboardInterrupt:
        print("\nDisconnected by user.")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    try:
        asyncio.run(probe_firehose())
    except KeyboardInterrupt:
        pass
