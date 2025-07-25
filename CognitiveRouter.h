#ifndef __TOMAHAWK6_COGNITIVEROUTER_H_
#define __TOMAHAWK6_COGNITIVEROUTER_H_

#include <omnetpp.h>
#include <map>
#include <vector>
#include <unordered_map>
#include "inet/common/INETDefs.h"
#include "inet/common/packet/Packet.h"

using namespace omnetpp;
using namespace inet;

namespace tomahawk6 {

/**
 * Cognitive Routing 2.0 implementation for Tomahawk 6
 * Includes adaptive routing, congestion control, and AI optimizations
 */
class INET_API CognitiveRouter : public cSimpleModule
{
  public:
    struct FlowInfo {
        simtime_t startTime;
        long totalBytes;
        int packetCount;
        double avgLatency;
        std::vector<int> pathHistory;
        bool isAITraffic;
    };
    
    struct PathMetrics {
        double utilization;
        simtime_t latency;
        double congestionLevel;
        int failureCount;
        simtime_t lastFailureTime;
    };

  private:
    // Configuration parameters
    bool adaptiveRouting;
    bool congestionControl;
    bool loadBalancing;
    simtime_t routingLatency;
    
    // Cognitive Routing 2.0 features
    bool advancedTelemetry;
    bool dynamicCongestionControl;
    bool rapidFailureDetection;
    bool packetTrimming;
    
    // Routing state
    std::unordered_map<std::string, FlowInfo> activeFlows;
    std::vector<PathMetrics> pathMetrics;
    std::map<int, std::vector<int>> routingTable;
    
    // Congestion control
    std::vector<double> portUtilization;
    std::vector<int> queueDepths;
    double congestionThreshold;
    
    // Load balancing
    std::vector<int> pathWeights;
    int currentRoundRobinIndex;
    
    // Failure detection
    simtime_t failureDetectionInterval;
    cMessage *failureDetectionTimer;
    std::vector<simtime_t> lastPortActivity;
    
    // Telemetry and statistics
    simsignal_t routingDecisionSignal;
    simsignal_t congestionLevelSignal;
    simsignal_t adaptiveRoutingSignal;
    simsignal_t loadBalancingSignal;
    
    // Timers
    cMessage *telemetryTimer;
    cMessage *congestionUpdateTimer;
    
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
    // Core routing functions
    virtual int selectOutputPort(cPacket *packet);
    virtual void updateRoutingDecision(cPacket *packet, int selectedPort);
    virtual std::string extractFlowId(cPacket *packet);
    
    // Adaptive routing
    virtual int adaptiveRoutingDecision(cPacket *packet, const std::string& flowId);
    virtual void updatePathMetrics(int port, cPacket *packet);
    virtual double calculatePathScore(int port, bool isAITraffic);
    
    // Congestion control
    virtual void updateCongestionMetrics();
    virtual bool isPortCongested(int port);
    virtual void applyCongestionControl(cPacket *packet, int port);
    virtual void performPacketTrimming(cPacket *packet);
    
    // Load balancing
    virtual int loadBalancedSelection(const std::vector<int>& candidatePorts);
    virtual void updateLoadBalancingWeights();
    
    // Failure detection and recovery
    virtual void performFailureDetection();
    virtual void handlePortFailure(int port);
    virtual bool isPortHealthy(int port);
    
    // AI traffic optimization
    virtual bool isAITraffic(cPacket *packet);
    virtual void optimizeForAIWorkload(cPacket *packet, FlowInfo& flow);
    
    // Telemetry
    virtual void collectTelemetryData();
    virtual void reportAdvancedMetrics();
    
  public:
    CognitiveRouter();
    virtual ~CognitiveRouter();
    
    // Public interface for external queries
    double getPortUtilization(int port) const;
    double getCongestionLevel(int port) const;
    PathMetrics getPathMetrics(int port) const;
};

} // namespace tomahawk6

#endif