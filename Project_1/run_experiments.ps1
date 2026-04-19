$producers = 4
$consumersList = @(1, 2, 3, 4, 6, 8, 12, 16, 32)
$iterations = 10 # 10 runs per configuration to average out the non-deterministic nature of the thread scheduling
$executable = ".\prod-cons.exe"

# If operating on WSL or standard bash without .exe suffix
if (!(Test-Path $executable)) {
    $executable = ".\prod-cons"
}

if (!(Test-Path $executable)) {
    Write-Host "Executable not found! Please run 'gcc -o prod-cons prod-cons.c -lpthread -lm' first." -ForegroundColor Red
    exit
}

Write-Host "Starting experiments..." -ForegroundColor Cyan
Write-Host "Fixed Producers (p) = $producers"
Write-Host "Runs per configuration = $iterations"
Write-Host "=================================================="

# Table Header
"| {0,-13} | {1,-28} |" -f "Consumers (q)", "Avg System Wait Time (μs)"
"|---------------|------------------------------|"

$results = @()

foreach ($q in $consumersList) {
    $sumWaitTime = 0.0
    
    for ($i = 1; $i -le $iterations; $i++) {
        # Execute the program and capture the output
        $output = & $executable $producers $q
        
        # Parse the average waiting time using regex
        $matched = $false
        foreach ($line in $output) {
            if ($line -match "Average waiting time in queue:\s*([\d.]+)") {
                $waitTime = [double]$Matches[1]
                $sumWaitTime += $waitTime
                $matched = $true
                break
            }
        }
        
        if (-not $matched) {
            Write-Host "Failed to parse output format in run $i" -ForegroundColor Red
        }
    }
    
    # Calculate the average over the 10 iterations
    $finalAvgWaitTime = $sumWaitTime / $iterations
    
    # Format and print the row
    $formattedAvg = "{0:N2}" -f $finalAvgWaitTime
    "| {0,-13} | {1,-28} |" -f $q, $formattedAvg
}

"=================================================="
Write-Host "Experiments finished!" -ForegroundColor Green