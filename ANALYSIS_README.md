# Tomahawk 6 Simulation Analysis Tools

This directory contains comprehensive analysis tools for the Tomahawk 6 Ethernet switch simulation results.

## 📊 Analysis Tools

### 1. Python Analysis Script (`analyze_results.py`)

A comprehensive Python script that parses OMNeT++ simulation results and provides detailed performance analysis.

**Features:**
- ✅ **Automatic result parsing** from `.sca` (scalar) files
- ✅ **Performance metrics calculation** (throughput, packet loss, utilization)
- ✅ **Tomahawk 6-specific analysis** (capacity utilization, performance rating)
- ✅ **Visualization generation** (performance comparison charts)
- ✅ **Multiple output formats** (text report, CSV data, PNG charts)

**Usage:**
```bash
# Basic analysis
python3 analyze_results.py

# Custom results directory
python3 analyze_results.py --dir my_results

# Verbose output
python3 analyze_results.py --verbose

# Skip chart generation
python3 analyze_results.py --no-charts
```

**Requirements:**
```bash
pip install pandas matplotlib seaborn numpy
# or
pip install -r requirements.txt
```

### 2. Analysis Suite Script (`run_analysis_suite.sh`)

Automated test runner that executes multiple simulation scenarios and performs comprehensive analysis.

**Features:**
- ✅ **Multiple configuration testing** (different simulation times)
- ✅ **Automated result archiving**
- ✅ **Comprehensive reporting**
- ✅ **Error handling and timeouts**

**Usage:**
```bash
./run_analysis_suite.sh
```

## 📈 Output Files

After running the analysis, you'll find:

### Generated Reports
- **`analysis_report.txt`** - Detailed text analysis report
- **`simulation_metrics.csv`** - Raw metrics data for further analysis
- **`performance_analysis.png`** - Performance comparison charts

### Example Output Structure
```
results/
├── BasicTest_1s.sca           # Simulation results
├── BasicTest_5s.sca
├── BasicTest_10s.sca
├── analysis_report.txt        # Generated analysis
├── simulation_metrics.csv     # Metrics data
└── performance_analysis.png   # Charts
```

## 🔍 Analysis Metrics

### Traffic Statistics
- **Packets Sent/Received** - Total packet counts
- **Packet Loss Rate** - Percentage of dropped packets
- **Data Volume** - Total bytes transferred

### Performance Analysis
- **Throughput** - Data rate in bytes/sec, Mbps, and Gbps
- **Capacity Utilization** - Percentage of Tomahawk 6's 102.4 Tbps capacity
- **Performance Rating** - Qualitative assessment (BASELINE/FAIR/GOOD/EXCELLENT)

### Tomahawk 6 Specific
- **Switch Capacity Analysis** - Utilization vs 102.4 Tbps specification
- **Port Configuration Impact** - Analysis of different SerDes configurations
- **AI Workload Performance** - Metrics specific to AI/ML traffic patterns

## 📊 Sample Analysis Output

```
TOMAHAWK 6 SIMULATION ANALYSIS RESULTS
============================================================

Configuration: BasicTest
----------------------------------------
Traffic Statistics:
  Packets Sent:      10,000
  Packets Received:  10,000
  Packet Loss Rate:  0.00%
  Total Data:        10.00 MB

Throughput Analysis:
  Throughput:        1,000,000 bytes/sec
  Throughput:        8.00 Mbps
  Throughput:        0.008 Gbps

Tomahawk 6 Performance Analysis:
  Switch Capacity:   102.4 Tbps (102,400 Gbps)
  Current Utilization: 0.000008%
  Performance Rating: BASELINE - Test traffic only
```

## 🚀 Advanced Usage

### Custom Analysis

You can extend the Python script to add custom metrics:

```python
# Add to analyze_results.py
def custom_analysis(self, config_name, metrics):
    # Your custom analysis code here
    pass
```

### Batch Processing

For analyzing large numbers of simulation runs:

```bash
# Run multiple configurations
for config in BasicTest HighLoadTest AITrainingWorkload; do
    ./tomahawk6_dbg -u Cmdenv -c $config --sim-time-limit=10s
done

# Analyze all results
python3 analyze_results.py
```

### Data Export

The CSV output can be imported into other analysis tools:

```python
import pandas as pd
df = pd.read_csv('results/simulation_metrics.csv')
# Further analysis with pandas, scipy, etc.
```

## 🛠 Troubleshooting

### Common Issues

1. **No results found**
   - Ensure simulations completed successfully
   - Check that `.sca` files exist in results directory

2. **Missing Python packages**
   - Install requirements: `pip install -r requirements.txt`

3. **Chart generation fails**
   - Use `--no-charts` flag to skip visualization
   - Check matplotlib backend configuration

4. **Permission errors**
   - Ensure scripts are executable: `chmod +x *.sh`

### Debug Mode

For detailed debugging:
```bash
python3 analyze_results.py --verbose
```

## 📝 Contributing

To add new analysis features:

1. Extend the `Tomahawk6Analyzer` class
2. Add new metric calculations in `calculate_performance_metrics()`
3. Update visualization in `generate_comparison_chart()`
4. Test with various simulation configurations

## 🔧 Integration

The analysis tools integrate with:
- **OMNeT++ simulation framework**
- **INET protocol library** 
- **Pandas data analysis library**
- **Matplotlib visualization library**
- **Standard Unix/Linux tools**

For questions or improvements, refer to the main simulation documentation.