#!/usr/bin/env python3
"""
Tomahawk 6 Simulation Results Analysis Tool

This script analyzes OMNeT++ simulation results from the Tomahawk 6 Ethernet switch
simulation, providing comprehensive performance metrics and visualization.

Author: AI Network Simulation Research
Version: 1.0
"""

import os
import re
import sys
import argparse
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from collections import defaultdict
import json
from datetime import datetime
import numpy as np

class Tomahawk6Analyzer:
    """Analyzes Tomahawk 6 simulation results from OMNeT++ scalar and vector files."""
    
    def __init__(self, results_dir="results"):
        self.results_dir = Path(results_dir)
        self.results = {}
        self.configs = {}
        self.metrics = defaultdict(list)
        
        # Set up plotting style
        plt.style.use('seaborn-v0_8' if 'seaborn-v0_8' in plt.style.available else 'default')
        sns.set_palette("husl")
        
    def load_scalar_files(self):
        """Load and parse OMNeT++ scalar (.sca) result files."""
        scalar_files = list(self.results_dir.glob("*.sca"))
        
        if not scalar_files:
            print(f"No .sca files found in {self.results_dir}")
            return
            
        print(f"Found {len(scalar_files)} scalar result files:")
        
        for sca_file in scalar_files:
            print(f"  - {sca_file.name}")
            config_name = sca_file.stem.split('-')[0]
            self.results[config_name] = self._parse_scalar_file(sca_file)
            
    def _parse_scalar_file(self, sca_file):
        """Parse individual .sca file and extract metrics."""
        results = {
            'config': {},
            'scalars': {},
            'parameters': {},
            'metadata': {}
        }
        
        try:
            with open(sca_file, 'r') as f:
                for line in f:
                    line = line.strip()
                    
                    if line.startswith('config '):
                        # Parse configuration parameters
                        parts = line.split(' ', 2)
                        if len(parts) >= 3:
                            key = parts[1]
                            value = parts[2].strip('"')
                            results['config'][key] = value
                            
                    elif line.startswith('scalar '):
                        # Parse scalar results - handle quotes properly
                        parts = line.split(' ', 2)
                        if len(parts) >= 3:
                            # Split remainder by quotes to handle metric names with spaces
                            remainder = parts[2]
                            if '"' in remainder:
                                # Handle quoted metric names
                                quote_start = remainder.find('"')
                                quote_end = remainder.find('"', quote_start + 1)
                                if quote_start != -1 and quote_end != -1:
                                    module = parts[1]
                                    metric = remainder[quote_start+1:quote_end]
                                    value_str = remainder[quote_end+1:].strip()
                                    
                                    try:
                                        value = float(value_str)
                                    except ValueError:
                                        value = value_str
                                        
                                    results['scalars'][f"{module}.{metric}"] = value
                            
                    elif line.startswith('par '):
                        # Parse parameters
                        parts = line.split(' ', 3)
                        if len(parts) >= 4:
                            module = parts[1]
                            param = parts[2]
                            value = parts[3].strip('"')
                            results['parameters'][f"{module}.{param}"] = value
                            
                    elif line.startswith('attr '):
                        # Parse metadata attributes
                        parts = line.split(' ', 2)
                        if len(parts) >= 3:
                            key = parts[1]
                            value = parts[2]
                            results['metadata'][key] = value
                            
        except Exception as e:
            print(f"Error parsing {sca_file}: {e}")
            
        return results
    
    def calculate_performance_metrics(self):
        """Calculate key performance metrics from raw simulation data."""
        print("\n" + "="*60)
        print("TOMAHAWK 6 SIMULATION ANALYSIS RESULTS")
        print("="*60)
        
        for config_name, data in self.results.items():
            print(f"\nConfiguration: {config_name}")
            print("-" * 40)
            
            scalars = data['scalars']
            config = data['config']
            
            
            # Extract key metrics (handle different possible module naming patterns)
            packets_sent = (scalars.get('BasicTomahawk6Test.source.Packets Sent', 0) or
                          scalars.get('source.Packets Sent', 0))
            packets_received = (scalars.get('BasicTomahawk6Test.sink.Packets Received', 0) or
                              scalars.get('sink.Packets Received', 0))
            total_bytes = (scalars.get('BasicTomahawk6Test.sink.Total Bytes', 0) or
                         scalars.get('sink.Total Bytes', 0))
            throughput = (scalars.get('BasicTomahawk6Test.sink.Throughput (bytes/sec)', 0) or
                        scalars.get('sink.Throughput (bytes/sec)', 0))
            
            # Calculate derived metrics
            packet_loss_rate = 0
            if packets_sent > 0:
                packet_loss_rate = ((packets_sent - packets_received) / packets_sent) * 100
                
            # Convert throughput to different units
            throughput_mbps = (throughput * 8) / 1e6  # Convert bytes/sec to Mbps
            throughput_gbps = throughput_mbps / 1000   # Convert to Gbps
            
            # Get simulation time
            sim_time = config.get('sim-time-limit', 'Unknown')
            
            # Store metrics for comparison
            self.metrics[config_name] = {
                'packets_sent': packets_sent,
                'packets_received': packets_received,
                'total_bytes': total_bytes,
                'throughput_bps': throughput,
                'throughput_mbps': throughput_mbps,
                'throughput_gbps': throughput_gbps,
                'packet_loss_rate': packet_loss_rate,
                'sim_time': sim_time
            }
            
            # Print performance summary
            print(f"Traffic Statistics:")
            print(f"  Packets Sent:      {packets_sent:,}")
            print(f"  Packets Received:  {packets_received:,}")
            print(f"  Packet Loss Rate:  {packet_loss_rate:.2f}%")
            print(f"  Total Data:        {total_bytes/1e6:.2f} MB")
            
            print(f"\nThroughput Analysis:")
            print(f"  Throughput:        {throughput:,.0f} bytes/sec")
            print(f"  Throughput:        {throughput_mbps:.2f} Mbps")
            print(f"  Throughput:        {throughput_gbps:.3f} Gbps")
            
            print(f"\nSimulation Parameters:")
            print(f"  Simulation Time:   {sim_time}")
            print(f"  Network:           {config.get('network', 'Unknown')}")
            
            # Tomahawk 6 specific analysis
            self._analyze_tomahawk6_performance(config_name, throughput_gbps)
            
    def _analyze_tomahawk6_performance(self, config_name, throughput_gbps):
        """Analyze performance relative to Tomahawk 6 specifications."""
        print(f"\nTomahawk 6 Performance Analysis:")
        
        # Tomahawk 6 specifications
        tomahawk6_capacity_tbps = 102.4
        tomahawk6_capacity_gbps = tomahawk6_capacity_tbps * 1000
        
        # Calculate utilization
        utilization = (throughput_gbps / tomahawk6_capacity_gbps) * 100
        
        print(f"  Switch Capacity:   {tomahawk6_capacity_tbps} Tbps ({tomahawk6_capacity_gbps:,.0f} Gbps)")
        print(f"  Current Utilization: {utilization:.6f}%")
        
        # Performance rating
        if utilization > 80:
            rating = "EXCELLENT - High utilization"
        elif utilization > 50:
            rating = "GOOD - Moderate utilization"
        elif utilization > 10:
            rating = "FAIR - Low utilization"
        else:
            rating = "BASELINE - Test traffic only"
            
        print(f"  Performance Rating: {rating}")
        
    def generate_comparison_chart(self):
        """Generate comparison charts for multiple configurations."""
        if len(self.metrics) < 1:
            print("No metrics available for comparison")
            return
            
        # Create comparison DataFrame
        df_data = []
        for config, metrics in self.metrics.items():
            df_data.append({
                'Configuration': config,
                'Throughput (Gbps)': metrics['throughput_gbps'],
                'Packet Loss (%)': metrics['packet_loss_rate'],
                'Packets Sent': metrics['packets_sent'],
                'Total Data (MB)': metrics['total_bytes'] / 1e6
            })
            
        df = pd.DataFrame(df_data)
        
        # Create subplots
        fig, axes = plt.subplots(2, 2, figsize=(15, 12))
        fig.suptitle('Tomahawk 6 Simulation Performance Analysis', fontsize=16, fontweight='bold')
        
        # Throughput comparison
        axes[0,0].bar(df['Configuration'], df['Throughput (Gbps)'], color='skyblue', alpha=0.8)
        axes[0,0].set_title('Throughput Comparison')
        axes[0,0].set_ylabel('Throughput (Gbps)')
        axes[0,0].tick_params(axis='x', rotation=45)
        
        # Packet loss comparison
        axes[0,1].bar(df['Configuration'], df['Packet Loss (%)'], color='lightcoral', alpha=0.8)
        axes[0,1].set_title('Packet Loss Rate')
        axes[0,1].set_ylabel('Packet Loss (%)')
        axes[0,1].tick_params(axis='x', rotation=45)
        
        # Traffic volume
        axes[1,0].bar(df['Configuration'], df['Packets Sent'], color='lightgreen', alpha=0.8)
        axes[1,0].set_title('Traffic Volume (Packets)')
        axes[1,0].set_ylabel('Packets Sent')
        axes[1,0].tick_params(axis='x', rotation=45)
        
        # Data volume
        axes[1,1].bar(df['Configuration'], df['Total Data (MB)'], color='gold', alpha=0.8)
        axes[1,1].set_title('Data Volume')
        axes[1,1].set_ylabel('Total Data (MB)')
        axes[1,1].tick_params(axis='x', rotation=45)
        
        plt.tight_layout()
        
        # Save chart
        chart_file = self.results_dir / 'performance_analysis.png'
        plt.savefig(chart_file, dpi=300, bbox_inches='tight')
        print(f"\nPerformance chart saved to: {chart_file}")
        
        return fig
        
    def generate_report(self):
        """Generate a comprehensive analysis report."""
        report_file = self.results_dir / 'analysis_report.txt'
        
        with open(report_file, 'w') as f:
            f.write("TOMAHAWK 6 SIMULATION ANALYSIS REPORT\n")
            f.write("=" * 50 + "\n")
            f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
            
            # Summary statistics
            f.write("SUMMARY STATISTICS\n")
            f.write("-" * 20 + "\n")
            
            total_configs = len(self.metrics)
            total_packets = sum(m['packets_sent'] for m in self.metrics.values())
            avg_throughput = np.mean([m['throughput_gbps'] for m in self.metrics.values()])
            max_throughput = max([m['throughput_gbps'] for m in self.metrics.values()])
            
            f.write(f"Configurations Analyzed: {total_configs}\n")
            f.write(f"Total Packets Processed: {total_packets:,}\n")
            f.write(f"Average Throughput: {avg_throughput:.3f} Gbps\n")
            f.write(f"Peak Throughput: {max_throughput:.3f} Gbps\n\n")
            
            # Detailed results
            f.write("DETAILED RESULTS\n")
            f.write("-" * 20 + "\n")
            
            for config_name, metrics in self.metrics.items():
                f.write(f"\n{config_name}:\n")
                f.write(f"  Packets: {metrics['packets_sent']:,} sent, {metrics['packets_received']:,} received\n")
                f.write(f"  Loss Rate: {metrics['packet_loss_rate']:.2f}%\n")
                f.write(f"  Throughput: {metrics['throughput_gbps']:.3f} Gbps\n")
                f.write(f"  Data Volume: {metrics['total_bytes']/1e6:.2f} MB\n")
                
        print(f"Analysis report saved to: {report_file}")
        
    def export_csv(self):
        """Export metrics to CSV for further analysis."""
        csv_file = self.results_dir / 'simulation_metrics.csv'
        
        # Prepare data for CSV
        csv_data = []
        for config, metrics in self.metrics.items():
            row = {'Configuration': config}
            row.update(metrics)
            csv_data.append(row)
            
        df = pd.DataFrame(csv_data)
        df.to_csv(csv_file, index=False)
        print(f"Metrics exported to CSV: {csv_file}")
        
    def run_analysis(self):
        """Run complete analysis pipeline."""
        print("Starting Tomahawk 6 simulation analysis...")
        
        # Load data
        self.load_scalar_files()
        
        if not self.results:
            print("No simulation results found to analyze.")
            return
            
        # Calculate metrics
        self.calculate_performance_metrics()
        
        # Generate visualizations
        try:
            self.generate_comparison_chart()
        except Exception as e:
            print(f"Could not generate charts: {e}")
            
        # Generate reports
        self.generate_report()
        self.export_csv()
        
        print(f"\nAnalysis complete! Check the {self.results_dir} directory for:")
        print("  - analysis_report.txt (detailed text report)")
        print("  - simulation_metrics.csv (metrics data)")
        print("  - performance_analysis.png (performance charts)")

def main():
    """Main function with command line interface."""
    parser = argparse.ArgumentParser(
        description="Analyze Tomahawk 6 simulation results",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python analyze_results.py                    # Analyze results/ directory
  python analyze_results.py --dir my_results   # Analyze custom directory
  python analyze_results.py --no-charts        # Skip chart generation
        """
    )
    
    parser.add_argument(
        '--dir', '-d',
        default='results',
        help='Directory containing simulation results (default: results)'
    )
    
    parser.add_argument(
        '--no-charts',
        action='store_true',
        help='Skip chart generation'
    )
    
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Enable verbose output'
    )
    
    args = parser.parse_args()
    
    # Check if results directory exists
    if not os.path.exists(args.dir):
        print(f"Error: Results directory '{args.dir}' not found.")
        sys.exit(1)
        
    # Create analyzer and run analysis
    analyzer = Tomahawk6Analyzer(args.dir)
    
    try:
        analyzer.run_analysis()
    except KeyboardInterrupt:
        print("\nAnalysis interrupted by user.")
        sys.exit(1)
    except Exception as e:
        print(f"Error during analysis: {e}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()