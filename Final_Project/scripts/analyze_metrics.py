import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# Ensure the metrics file exists before proceeding
metrics_file = '../metrics_log.txt'
if not os.path.exists(metrics_file):
    print("Error: metrics_log.txt not found. Have you executed the C application yet?")
    exit(1)

print("Loading Real-Time Analytics from:", metrics_file)

# Load the CSV
try:
    df = pd.read_csv(metrics_file)
except Exception as e:
    print(f"Error loading CSV data: {e}")
    exit(1)

# Ensure columns are what we expect
expected_columns = ['Seconds', 'Nanoseconds', 'Commit_Count', 'Identity_Count', 'Account_Count', 'Info_Count', 'Buffer_Occupancy_Pct', 'CPU_Pct']
if list(df.columns) != expected_columns:
    print(f"Warning: Columns might not match perfectly.\nExpected: {expected_columns}\nFound: {list(df.columns)}")

# Calculate the Jitter
# Ideal delta is exactly 1.000000000 seconds (1,000,000,000 ns).
# First, create a continuous precise time array in absolute nanoseconds
df['Abs_Time_ns'] = df['Seconds'] * 1e9 + df['Nanoseconds']
# Delta between consecutive periodic wakeups
df['Delta_ns'] = df['Abs_Time_ns'].diff()
# Jitter is the difference between actual delta and exactly 1 second (in milliseconds)
df['Jitter_ms'] = (df['Delta_ns'] - 1e9) / 1e6

# Calculate total Hz (messages per second per 1-second window)
df['Messages_Hz'] = df['Commit_Count'] + df['Identity_Count'] + df['Account_Count'] + df['Info_Count']

# --- Plot 1: Jitter Plot ---
plt.figure(figsize=(10, 5))
plt.plot(df.index, df['Jitter_ms'], color='blue', alpha=0.7)
plt.axhline(0, color='red', linestyle='--', linewidth=1)
plt.title('Jitter of the Periodic Monitor Thread')
plt.xlabel('Execution Window (Seconds)')
plt.ylabel('Jitter (Milliseconds)')
plt.grid(True)
plt.tight_layout()
plt.savefig('Chart_1_Jitter_Analysis.png')
print("Saved Chart_1_Jitter_Analysis.png")

# --- Plot 2: Burstiness (Hz vs Buffer) ---
fig, ax1 = plt.subplots(figsize=(10, 5))
color1 = 'tab:orange'
ax1.set_xlabel('Execution Window (Seconds)')
ax1.set_ylabel('Messages (Hz)', color=color1)
ax1.plot(df.index, df['Messages_Hz'], color=color1, alpha=0.8, label="Total Messages")
ax1.tick_params(axis='y', labelcolor=color1)

ax2 = ax1.twinx()  
color2 = 'tab:green'
ax2.set_ylabel('Buffer Occupancy (%)', color=color2)  
ax2.plot(df.index, df['Buffer_Occupancy_Pct'], color=color2, alpha=0.6, label="Buffer Fill %")
ax2.tick_params(axis='y', labelcolor=color2)
ax2.set_ylim(0, 100) # Buffer is strictly 0 to 100%

fig.tight_layout()  
plt.title('Network Burstiness vs Ring Buffer Occupancy')
plt.savefig('Chart_2_Burstiness.png')
print("Saved Chart_2_Burstiness.png")

# --- Plot 3: Network Hz vs CPU Usage ---
fig, ax1 = plt.subplots(figsize=(10, 5))
color1 = 'tab:orange'
ax1.set_xlabel('Execution Window (Seconds)')
ax1.set_ylabel('Messages (Hz)', color=color1)
ax1.plot(df.index, df['Messages_Hz'], color=color1, alpha=0.8)
ax1.tick_params(axis='y', labelcolor=color1)

ax2 = ax1.twinx()  
color2 = 'tab:purple'
ax2.set_ylabel('CPU Usage (%)', color=color2)  
ax2.plot(df.index, df['CPU_Pct'], color=color2, alpha=0.6)
ax2.tick_params(axis='y', labelcolor=color2)
ax2.set_ylim(0, 100) 

fig.tight_layout()  
plt.title('Incoming Messages Rate (Hz) vs CPU Utilization')
plt.savefig('Chart_3_CPU_Usage.png')
print("Saved Chart_3_CPU_Usage.png")

print("Analysis successfully rendered!")
