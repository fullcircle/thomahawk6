#!/bin/bash
#
# Tomahawk 6 Analysis Suite
# Runs multiple simulation configurations and analyzes results
#

set -e

echo "========================================"
echo "Tomahawk 6 Analysis Suite"
echo "========================================"

# Set OMNeT++ environment
export OMNETPP_ROOT="/mnt/d/omnetpp-6.2.0"
export PATH="$OMNETPP_ROOT/bin:$PATH"

# Configurations to test
CONFIGS=(
    "BasicTest"
)

# Simulation parameters
SIM_TIMES=("1s" "5s" "10s")

echo "Running simulation scenarios..."

# Clean old results
rm -rf results_archive
mkdir -p results_archive
if [ -d results ]; then
    mv results/* results_archive/ 2>/dev/null || true
fi

# Run simulations with different parameters
for config in "${CONFIGS[@]}"; do
    for sim_time in "${SIM_TIMES[@]}"; do
        scenario_name="${config}_${sim_time}"
        echo ""
        echo "Running scenario: $scenario_name"
        echo "---------------------------------"
        
        # Run simulation
        timeout 60s ./tomahawk6_dbg -u Cmdenv -c "$config" \
            --sim-time-limit="$sim_time" \
            --output-scalar-file="results/${scenario_name}.sca" \
            --output-vector-file="results/${scenario_name}.vec"
        
        if [ $? -eq 0 ]; then
            echo "✓ $scenario_name completed successfully"
        else
            echo "✗ $scenario_name failed or timed out"
        fi
    done
done

echo ""
echo "Running comprehensive analysis..."
echo "================================="

# Run Python analysis
if command -v python3 &> /dev/null; then
    python3 analyze_results.py --verbose
    
    echo ""
    echo "Analysis Results Summary:"
    echo "========================"
    
    if [ -f results/analysis_report.txt ]; then
        echo "📊 Analysis Report:"
        head -20 results/analysis_report.txt
        echo ""
        echo "... (see results/analysis_report.txt for full report)"
    fi
    
    if [ -f results/simulation_metrics.csv ]; then
        echo ""
        echo "📈 CSV Metrics available at: results/simulation_metrics.csv"
    fi
    
    if [ -f results/performance_analysis.png ]; then
        echo "📊 Performance charts saved to: results/performance_analysis.png"
    fi
    
else
    echo "Python3 not available, skipping detailed analysis"
fi

echo ""
echo "========================================"
echo "Analysis Suite Complete!"
echo "========================================"
echo ""
echo "Results available in:"
echo "  - results/ (current run)"
echo "  - results_archive/ (previous runs)"
echo ""
echo "Key files:"
echo "  - analysis_report.txt (detailed analysis)"
echo "  - simulation_metrics.csv (raw data)"
echo "  - performance_analysis.png (charts)"