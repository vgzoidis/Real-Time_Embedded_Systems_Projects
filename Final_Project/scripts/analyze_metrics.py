import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import numpy as np
import os
from pathlib import Path
from datetime import datetime

# Ensure the metrics file exists before proceeding
script_dir = Path(__file__).resolve().parent
project_root = script_dir.parent
metrics_file = project_root / 'metrics_log.txt'
if not metrics_file.exists():
    print("Error: metrics_log.txt not found. Have you executed the C application yet?")
    exit(1)

print("Loading Real-Time Analytics from:", metrics_file)

# Load the CSV
try:
    expected_columns = ['Seconds', 'Nanoseconds', 'Commit_Count', 'Identity_Count', 'Account_Count', 'Info_Count', 'Buffer_Occupancy_Pct', 'CPU_Pct']
    df = pd.read_csv(metrics_file, header=None, names=expected_columns)
except Exception as e:
    print(f"Error loading CSV data: {e}")
    exit(1)

# Coerce malformed or partially written rows to NaN, then drop them before plotting.
# This keeps the analytics resilient if the logger is interrupted while appending.
for column in expected_columns:
    df[column] = pd.to_numeric(df[column], errors='coerce')

df = df.dropna(subset=['Seconds', 'Nanoseconds', 'Commit_Count', 'Identity_Count', 'Account_Count', 'Info_Count', 'Buffer_Occupancy_Pct', 'CPU_Pct']).copy()
if df.empty:
    print("Error: metrics_log.txt did not contain any valid data rows.")
    exit(1)

# Calculate the Jitter
# Ideal delta is exactly 1.000000000 seconds (1,000,000,000 ns).
# First, create a continuous precise time array in absolute nanoseconds
df['Abs_Time_ns'] = df['Seconds'] * 1e9 + df['Nanoseconds']
# Delta between consecutive periodic wakeups
df['Delta_ns'] = df['Abs_Time_ns'].diff()

# If the user restarts the C application, the gap between the last run's timestamp 
# and the new run's timestamp will be massive. We filter out any delta > 5 seconds 
# to prevent application restarts from causing artificial, massive spikes in the Jitter graph.
df.loc[df['Delta_ns'] > 5e9, 'Delta_ns'] = np.nan

# Jitter is the difference between actual delta and exactly 1 second (in milliseconds)
df['Jitter_ms'] = (df['Delta_ns'] - 1e9) / 1e6

# Calculate total Hz (messages per second per 1-second window)
df['Messages_Hz'] = df['Commit_Count'] + df['Identity_Count'] + df['Account_Count'] + df['Info_Count']

# Convert Unix time to human-readable local time so the plots reflect the user's timezone.
local_timezone = datetime.now().astimezone().tzinfo
df['Local_Timestamp'] = pd.to_datetime(df['Seconds'], unit='s', utc=True).dt.tz_convert(local_timezone)
df = df.dropna(subset=['Local_Timestamp']).copy()
if df.empty:
    print("Error: no valid timestamps were found in metrics_log.txt.")
    exit(1)
start_local_time = df['Local_Timestamp'].iloc[0]
end_local_time = df['Local_Timestamp'].iloc[-1]

# --- Plot 1: Jitter Plot ---
plt.figure(figsize=(10, 5))
plt.plot(df['Local_Timestamp'], df['Jitter_ms'], color='blue', alpha=0.7)
plt.axhline(0, color='red', linestyle='--', linewidth=1)
plt.title('Jitter of the Periodic Monitor Thread')
plt.xlabel('Execution Window (Local Time)')
plt.ylabel('Jitter (Milliseconds)')
plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S', tz=local_timezone))
plt.gca().xaxis.set_major_locator(mdates.AutoDateLocator(minticks=4, maxticks=10))
plt.gcf().autofmt_xdate()
plt.grid(True)
plt.tight_layout()
docs_dir = project_root / 'docs'
plt.savefig(docs_dir / 'Chart_1_Jitter_Analysis.png')
print("Saved Chart_1_Jitter_Analysis.png")

# --- Plot 2: Burstiness (Hz vs Buffer) ---
fig, ax1 = plt.subplots(figsize=(10, 5))
color1 = 'tab:orange'
ax1.set_xlabel('Execution Window (Local Time)')
ax1.set_ylabel('Messages (Hz)', color=color1)
ax1.plot(df['Local_Timestamp'], df['Messages_Hz'], color=color1, alpha=0.8, label="Total Messages")
ax1.tick_params(axis='y', labelcolor=color1)
ax1.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S', tz=local_timezone))
ax1.xaxis.set_major_locator(mdates.AutoDateLocator(minticks=4, maxticks=10))

ax2 = ax1.twinx()  
color2 = 'tab:green'
ax2.set_ylabel('Buffer Occupancy (%)', color=color2)  
ax2.plot(df['Local_Timestamp'], df['Buffer_Occupancy_Pct'], color=color2, alpha=0.75, label="Buffer Fill %")
ax2.tick_params(axis='y', labelcolor=color2)

# Zoom the green axis to the observed occupancy so low-level activity stays visible.
buffer_max = float(df['Buffer_Occupancy_Pct'].max())
buffer_upper = max(1.0, buffer_max * 1.25)
if buffer_upper < 5.0:
    buffer_upper = 5.0
ax2.set_ylim(0, buffer_upper)

fig.tight_layout()  
plt.title('Network Burstiness vs Ring Buffer Occupancy')
plt.savefig(docs_dir / 'Chart_2_Burstiness.png')
print("Saved Chart_2_Burstiness.png")

# --- Plot 3: Network Hz vs CPU Usage ---
fig, ax1 = plt.subplots(figsize=(10, 5))
color1 = 'tab:orange'
ax1.set_xlabel('Execution Window (Local Time)')
ax1.set_ylabel('Messages (Hz)', color=color1)
ax1.plot(df['Local_Timestamp'], df['Messages_Hz'], color=color1, alpha=0.8)
ax1.tick_params(axis='y', labelcolor=color1)
ax1.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S', tz=local_timezone))
ax1.xaxis.set_major_locator(mdates.AutoDateLocator(minticks=4, maxticks=10))

ax2 = ax1.twinx()  
color2 = 'tab:purple'
ax2.set_ylabel('CPU Usage (%)', color=color2)  
ax2.plot(df['Local_Timestamp'], df['CPU_Pct'], color=color2, alpha=0.6)
ax2.tick_params(axis='y', labelcolor=color2)
ax2.set_ylim(0, 100) 

fig.tight_layout()  
plt.title('Incoming Messages Rate (Hz) vs CPU Utilization')
plt.savefig(docs_dir / 'Chart_3_CPU_Usage.png')
print("Saved Chart_3_CPU_Usage.png")

# --- Export Statistical Summary for LaTeX ---
# Calculate key metrics to prove robustness
jitter_median = df['Jitter_ms'].median()
jitter_75th = df['Jitter_ms'].quantile(0.75)
jitter_mean = df['Jitter_ms'].mean()
cpu_mean = df['CPU_Pct'].mean()
cpu_max = df['CPU_Pct'].max()
buffer_max = df['Buffer_Occupancy_Pct'].max()

stats_tex = rf"""
\\begin{{itemize}}
    \\item \\textbf{{Median Jitter:}} {jitter_median:.5f} ms (Fractional microseconds).
    \\item \\textbf{{75th Percentile Jitter:}} {jitter_75th:.5f} ms.
    \\item \\textbf{{Mean Average Jitter:}} {jitter_mean:.5f} ms.
    \\item \\textbf{{Average CPU Load:}} {cpu_mean:.2f}\\%.
    \\item \\textbf{{Maximum CPU Load:}} {cpu_max:.2f}\\%.
    \\item \\textbf{{Peak Buffer Occupancy:}} {buffer_max:.2f}\\%.
    \item \textbf{{Analysis Window (Local Time):}} {start_local_time.strftime('%Y-%m-%d %H:%M:%S %Z')} to {end_local_time.strftime('%Y-%m-%d %H:%M:%S %Z')}.
\\end{{itemize}}
"""

with open(docs_dir / 'stats_summary.tex', 'w') as f:
    f.write(stats_tex)
print("Saved statistics to stats_summary.tex")

print("Analysis successfully rendered!")
