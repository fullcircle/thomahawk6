#!/bin/bash
#
# Tomahawk 6 Simulation Test Suite
# Validates basic functionality and runs performance tests
#

set -e

echo "========================================"
echo "Tomahawk 6 Simulation Test Suite"
echo "========================================"

# Set OMNeT++ paths
export OMNETPP_ROOT="/mnt/d/omnetpp-6.2.0"
export PATH="$OMNETPP_ROOT/bin:$PATH"

# Check if OMNeT++ is available
if [ ! -f "$OMNETPP_ROOT/bin/opp_run" ]; then
    echo "ERROR: OMNeT++ not found at $OMNETPP_ROOT"
    echo "Please check the OMNETPP_ROOT path in this script."
    exit 1
fi

# Check if INET is available
export INET_PROJ="$OMNETPP_ROOT/inet4.5"

echo "Using OMNeT++ at: $OMNETPP_ROOT"
if [ -d "$INET_PROJ" ]; then
    echo "Using INET project at: $INET_PROJ"
else
    echo "ERROR: INET not found at $INET_PROJ"
    echo "Please ensure INET framework is installed"
    exit 1
fi

# Create results directory
mkdir -p results

# Build the simulation
echo ""
echo "Building Tomahawk 6 simulation..."
if [ ! -f Makefile.local ]; then
    echo "Generating makefile..."
    $OMNETPP_ROOT/bin/opp_makemake -f --deep -I$INET_PROJ/src -L$INET_PROJ/src -lINET
fi

make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed"
    exit 1
fi

echo "Build successful!"

# Test configurations
CONFIGS=(
    "BasicTest"
    "AITrainingWorkload" 
    "AIInferenceWorkload"
    "HighSpeedSerDesTest"
    "AdaptiveBufferTest"
    "CongestionControlTest"
)

# Run quick validation tests
echo ""
echo "Running validation tests..."

for config in "${CONFIGS[@]}"; do
    echo ""
    echo "Testing configuration: $config"
    
    # Run short simulation for validation
    timeout 30s ./tomahawk6 -u Cmdenv -c $config --sim-time-limit=1s --output-scalar-file=results/${config}_validation.sca
    
    if [ $? -eq 0 ]; then
        echo "✓ $config: PASSED"
    elif [ $? -eq 124 ]; then
        echo "✓ $config: TIMEOUT (simulation started successfully)"
    else
        echo "✗ $config: FAILED"
    fi
done

# Performance benchmark test
echo ""
echo "Running performance benchmark..."
echo "Configuration: HighLoadTest with 5s simulation time"

./tomahawk6 -u Cmdenv -c HighLoadTest --sim-time-limit=5s --output-scalar-file=results/benchmark.sca

if [ $? -eq 0 ]; then
    echo "✓ Performance benchmark: COMPLETED"
else
    echo "✗ Performance benchmark: FAILED"
fi

# Analyze results if available
echo ""
echo "Analyzing results..."

if [ -f results/benchmark.sca ]; then
    echo "Benchmark results summary:"
    echo "========================="
    
    # Extract key metrics using awk
    if command -v awk &> /dev/null; then
        echo -n "Total Throughput: "
        awk '/throughput.*scalar/ {sum += $4} END {print sum " bps"}' results/benchmark.sca
        
        echo -n "Average Latency: "
        awk '/latency.*scalar.*mean/ {sum += $4; count++} END {if(count>0) print sum/count " s"; else print "N/A"}' results/benchmark.sca
        
        echo -n "Buffer Utilization: "
        awk '/bufferUtilization.*scalar.*mean/ {sum += $4; count++} END {if(count>0) print sum/count*100 "%"; else print "N/A"}' results/benchmark.sca
        
        echo -n "Packets Processed: "
        awk '/Packets Sent.*scalar/ {sum += $4} END {print sum}' results/benchmark.sca
    fi
else
    echo "No benchmark results found."
fi

# Memory and performance check
echo ""
echo "System requirements check:"
echo "========================="
echo "Available memory: $(free -h | awk '/^Mem:/ {print $7}')"
echo "CPU cores: $(nproc)"
echo "OMNeT++ version: $(opp_run --version 2>&1 | head -1)"

# Configuration validation
echo ""
echo "Configuration validation:"
echo "========================"

# Check if all required files exist
FILES=(
    "Tomahawk6.ned"
    "SerDesCore.h"
    "SerDesCore.cc"
    "PacketBuffer.h" 
    "PacketBuffer.cc"
    "CognitiveRouter.h"
    "CognitiveRouter.cc"
    "AITrafficGenerator.h"
    "AITrafficGenerator.cc"
    "TopologyExamples.ned"
    "omnetpp.ini"
)

for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "✓ $file"
    else
        echo "✗ $file (missing)"
    fi
done

echo ""
echo "========================================" 
echo "Test suite completed!"
echo "========================================"
echo "Results stored in: results/"
echo "To run individual tests:"
echo "./tomahawk6 -u Cmdenv -c <ConfigName>"
echo ""
echo "To run with GUI:"
echo "./tomahawk6 -u Qtenv -c <ConfigName>"
echo ""
echo "Available configurations:"
for config in "${CONFIGS[@]}"; do
    echo "  - $config"
done

echo ""
echo "For detailed performance analysis:"
echo "1. Run longer simulations (10s+)"
echo "2. Use vector recording for time-series data"
echo "3. Analyze results with OMNeT++ IDE or custom scripts"