#include "CognitiveRouter.h"
#include "inet/common/packet/Packet.h"
#include <algorithm>
#include <sstream>

namespace tomahawk6 {

Define_Module(CognitiveRouter);

CognitiveRouter::CognitiveRouter()
{
    failureDetectionTimer = nullptr;
    telemetryTimer = nullptr;
    congestionUpdateTimer = nullptr;
    currentRoundRobinIndex = 0;
    congestionThreshold = 0.8;
    failureDetectionInterval = 0.001; // 1ms
}

CognitiveRouter::~CognitiveRouter()
{
    cancelAndDelete(failureDetectionTimer);
    cancelAndDelete(telemetryTimer);
    cancelAndDelete(congestionUpdateTimer);
}

void CognitiveRouter::initialize()
{
    // Read parameters
    adaptiveRouting = par("adaptiveRouting");
    congestionControl = par("congestionControl");
    loadBalancing = par("loadBalancing");
    routingLatency = par("routingLatency");
    
    // Cognitive Routing 2.0 features
    advancedTelemetry = par("advancedTelemetry");
    dynamicCongestionControl = par("dynamicCongestionControl");
    rapidFailureDetection = par("rapidFailureDetection");
    packetTrimming = par("packetTrimming");
    
    // Initialize data structures
    int numPorts = gateSize("out");
    pathMetrics.resize(numPorts);
    portUtilization.resize(numPorts, 0.0);
    queueDepths.resize(numPorts, 0);
    pathWeights.resize(numPorts, 1);
    lastPortActivity.resize(numPorts, simTime());
    
    // Initialize path metrics
    for (int i = 0; i < numPorts; i++) {
        pathMetrics[i].utilization = 0.0;
        pathMetrics[i].latency = 0.0;
        pathMetrics[i].congestionLevel = 0.0;
        pathMetrics[i].failureCount = 0;
        pathMetrics[i].lastFailureTime = 0;
    }
    
    // Initialize statistics signals
    routingDecisionSignal = registerSignal("routingDecision");
    congestionLevelSignal = registerSignal("congestionLevel");
    adaptiveRoutingSignal = registerSignal("adaptiveRouting");
    loadBalancingSignal = registerSignal("loadBalancing");
    
    // Setup timers
    if (rapidFailureDetection) {
        failureDetectionTimer = new cMessage("failureDetection");
        scheduleAt(simTime() + failureDetectionInterval, failureDetectionTimer);
    }
    
    if (advancedTelemetry) {
        telemetryTimer = new cMessage("telemetry");
        scheduleAt(simTime() + 0.01, telemetryTimer); // 10ms telemetry interval
    }
    
    if (dynamicCongestionControl) {
        congestionUpdateTimer = new cMessage("congestionUpdate");
        scheduleAt(simTime() + 0.001, congestionUpdateTimer); // 1ms congestion updates
    }
    
    EV << "CognitiveRouter initialized with " << numPorts << " ports" << endl;
    EV << "Features: Adaptive=" << adaptiveRouting 
       << ", Congestion=" << congestionControl 
       << ", LoadBalance=" << loadBalancing << endl;
}

void CognitiveRouter::handleMessage(cMessage *msg)
{
    if (msg == failureDetectionTimer) {
        performFailureDetection();
        scheduleAt(simTime() + failureDetectionInterval, failureDetectionTimer);
        return;
    }
    
    if (msg == telemetryTimer) {
        collectTelemetryData();
        scheduleAt(simTime() + 0.01, telemetryTimer);
        return;
    }
    
    if (msg == congestionUpdateTimer) {
        updateCongestionMetrics();
        scheduleAt(simTime() + 0.001, congestionUpdateTimer);
        return;
    }
    
    // Handle incoming packet
    cPacket *packet = check_and_cast<cPacket*>(msg);
    
    // Select output port using cognitive routing
    int selectedPort = selectOutputPort(packet);
    
    if (selectedPort == -1) {
        EV << "No available output port, dropping packet" << endl;
        delete packet;
        return;
    }
    
    // Apply congestion control if enabled
    if (congestionControl && isPortCongested(selectedPort)) {
        applyCongestionControl(packet, selectedPort);
    }
    
    // Update routing decision tracking
    updateRoutingDecision(packet, selectedPort);
    
    // Send packet with routing latency
    sendDelayed(packet, routingLatency, "out", selectedPort);
    
    // Update port activity tracking
    lastPortActivity[selectedPort] = simTime();
    
    emit(routingDecisionSignal, selectedPort);
}

int CognitiveRouter::selectOutputPort(cPacket *packet)
{
    std::string flowId = extractFlowId(packet);
    
    // Check if this is an active flow
    auto flowIt = activeFlows.find(flowId);
    if (flowIt == activeFlows.end()) {
        // New flow, create entry
        FlowInfo newFlow;
        newFlow.startTime = simTime();
        newFlow.totalBytes = 0;
        newFlow.packetCount = 0;
        newFlow.avgLatency = 0;
        newFlow.isAITraffic = isAITraffic(packet);
        activeFlows[flowId] = newFlow;
        flowIt = activeFlows.find(flowId);
    }
    
    // Update flow information
    flowIt->second.totalBytes += packet->getByteLength();
    flowIt->second.packetCount++;
    
    int selectedPort = -1;
    
    if (adaptiveRouting) {
        selectedPort = adaptiveRoutingDecision(packet, flowId);
        emit(adaptiveRoutingSignal, 1);
    } else {
        // Simple round-robin or hash-based routing
        selectedPort = currentRoundRobinIndex % gateSize("out");
        currentRoundRobinIndex++;
    }
    
    // Apply load balancing if enabled
    if (loadBalancing && selectedPort != -1) {
        std::vector<int> candidatePorts;
        
        // Find candidate ports with similar characteristics
        double selectedScore = calculatePathScore(selectedPort, flowIt->second.isAITraffic);
        for (int i = 0; i < gateSize("out"); i++) {
            double score = calculatePathScore(i, flowIt->second.isAITraffic);
            if (std::abs(score - selectedScore) < 0.1 && isPortHealthy(i)) {
                candidatePorts.push_back(i);
            }
        }
        
        if (!candidatePorts.empty()) {
            selectedPort = loadBalancedSelection(candidatePorts);
            emit(loadBalancingSignal, candidatePorts.size());
        }
    }
    
    // AI traffic optimization
    if (flowIt->second.isAITraffic) {
        optimizeForAIWorkload(packet, flowIt->second);
    }
    
    return selectedPort;
}

int CognitiveRouter::adaptiveRoutingDecision(cPacket *packet, const std::string& flowId)
{
    FlowInfo& flow = activeFlows[flowId];
    
    int bestPort = -1;
    double bestScore = -1.0;
    
    // Evaluate all available ports
    for (int port = 0; port < gateSize("out"); port++) {
        if (!isPortHealthy(port)) continue;
        
        double score = calculatePathScore(port, flow.isAITraffic);
        
        // Consider flow history for sticky routing
        bool wasUsedBefore = std::find(flow.pathHistory.begin(), 
                                      flow.pathHistory.end(), port) != flow.pathHistory.end();
        if (wasUsedBefore) {
            score *= 1.1; // Slight preference for previously used paths
        }
        
        if (score > bestScore) {
            bestScore = score;
            bestPort = port;
        }
    }
    
    // Update flow path history
    if (bestPort != -1) {
        flow.pathHistory.push_back(bestPort);
        if (flow.pathHistory.size() > 10) {
            flow.pathHistory.erase(flow.pathHistory.begin());
        }
    }
    
    return bestPort;
}

double CognitiveRouter::calculatePathScore(int port, bool isAITraffic)
{
    if (port < 0 || port >= (int)pathMetrics.size()) return 0.0;
    
    PathMetrics& metrics = pathMetrics[port];
    
    // Base score calculation
    double score = 1.0;
    
    // Penalize high utilization
    score *= (1.0 - metrics.utilization);
    
    // Penalize high latency
    if (metrics.latency > 0) {
        score *= (1.0 / (1.0 + metrics.latency.dbl() * 1000)); // Convert to ms
    }
    
    // Penalize congestion
    score *= (1.0 - metrics.congestionLevel);
    
    // Penalize recent failures
    if (metrics.failureCount > 0) {
        double timeSinceFailure = (simTime() - metrics.lastFailureTime).dbl();
        if (timeSinceFailure < 1.0) { // Within last second
            score *= 0.5;
        }
    }
    
    // AI traffic gets preference on less congested paths
    if (isAITraffic && metrics.congestionLevel < 0.3) {
        score *= 1.3;
    }
    
    return score;
}

void CognitiveRouter::updatePathMetrics(int port, cPacket *packet)
{
    if (port < 0 || port >= (int)pathMetrics.size()) return;
    
    PathMetrics& metrics = pathMetrics[port];
    
    // Update utilization (exponential moving average)
    double alpha = 0.1;
    double instantUtilization = (double)packet->getBitLength() / (100e9 * 0.001); // Assume 100G port, 1ms window
    metrics.utilization = alpha * instantUtilization + (1 - alpha) * metrics.utilization;
    
    // Update latency if packet has timestamp
    if (packet->getCreationTime() > 0) {
        simtime_t packetLatency = simTime() - packet->getCreationTime();
        metrics.latency = alpha * packetLatency + (1 - alpha) * metrics.latency;
    }
    
    // Update congestion level based on utilization and queue depth
    metrics.congestionLevel = std::min(1.0, metrics.utilization + queueDepths[port] * 0.01);
}

void CognitiveRouter::updateCongestionMetrics()
{
    for (int port = 0; port < gateSize("out"); port++) {
        // Update congestion level
        double congestion = portUtilization[port];
        if (queueDepths[port] > 10) {
            congestion += 0.2; // Queue building up
        }
        
        pathMetrics[port].congestionLevel = std::min(1.0, congestion);
        emit(congestionLevelSignal, pathMetrics[port].congestionLevel);
    }
    
    // Update load balancing weights
    if (loadBalancing) {
        updateLoadBalancingWeights();
    }
}

bool CognitiveRouter::isPortCongested(int port)
{
    if (port < 0 || port >= (int)pathMetrics.size()) return true;
    return pathMetrics[port].congestionLevel > congestionThreshold;
}

void CognitiveRouter::applyCongestionControl(cPacket *packet, int port)
{
    // Implement congestion control mechanisms
    
    // Packet trimming for large packets under congestion
    if (packetTrimming && packet->getByteLength() > 1500) {
        performPacketTrimming(packet);
    }
    
    // ECN marking simulation (would normally modify packet headers)
    EV << "Applying congestion control to packet on port " << port << endl;
}

void CognitiveRouter::performPacketTrimming(cPacket *packet)
{
    // Simulate packet trimming by reducing packet size
    long originalSize = packet->getByteLength();
    long trimmedSize = std::max(64L, originalSize * 3 / 4); // Trim to 75% minimum 64 bytes
    
    packet->setByteLength(trimmedSize);
    EV << "Packet trimmed from " << originalSize << " to " << trimmedSize << " bytes" << endl;
}

int CognitiveRouter::loadBalancedSelection(const std::vector<int>& candidatePorts)
{
    if (candidatePorts.empty()) return -1;
    
    // Weighted selection based on inverse utilization
    std::vector<double> weights;
    double totalWeight = 0;
    
    for (int port : candidatePorts) {
        double weight = 1.0 - portUtilization[port];
        weights.push_back(weight);
        totalWeight += weight;
    }
    
    if (totalWeight == 0) {
        // All ports equally loaded, use round-robin
        return candidatePorts[currentRoundRobinIndex % candidatePorts.size()];
    }
    
    // Weighted random selection
    double random = uniform(0, totalWeight);
    double cumulative = 0;
    
    for (size_t i = 0; i < candidatePorts.size(); i++) {
        cumulative += weights[i];
        if (random <= cumulative) {
            return candidatePorts[i];
        }
    }
    
    return candidatePorts.back();
}

void CognitiveRouter::updateLoadBalancingWeights()
{
    for (int port = 0; port < gateSize("out"); port++) {
        // Weight inversely proportional to utilization
        pathWeights[port] = std::max(1, (int)(10 * (1.0 - portUtilization[port])));
    }
}

void CognitiveRouter::performFailureDetection()
{
    simtime_t currentTime = simTime();
    
    for (int port = 0; port < gateSize("out"); port++) {
        // Check if port has been inactive for too long
        simtime_t inactiveTime = currentTime - lastPortActivity[port];
        
        if (inactiveTime > 0.1) { // 100ms threshold
            // Potential failure detected
            handlePortFailure(port);
        }
    }
}

void CognitiveRouter::handlePortFailure(int port)
{
    if (port < 0 || port >= (int)pathMetrics.size()) return;
    
    pathMetrics[port].failureCount++;
    pathMetrics[port].lastFailureTime = simTime();
    
    EV << "Port failure detected on port " << port 
       << ", failure count: " << pathMetrics[port].failureCount << endl;
}

bool CognitiveRouter::isPortHealthy(int port)
{
    if (port < 0 || port >= (int)pathMetrics.size()) return false;
    
    // Port is healthy if it hasn't failed recently
    simtime_t timeSinceFailure = simTime() - pathMetrics[port].lastFailureTime;
    return timeSinceFailure > 1.0 || pathMetrics[port].failureCount == 0;
}

bool CognitiveRouter::isAITraffic(cPacket *packet)
{
    std::string packetName = packet->getName();
    return packetName.find("AI") != std::string::npos ||
           packetName.find("AllReduce") != std::string::npos ||
           packetName.find("AllGather") != std::string::npos ||
           packetName.find("RoCE") != std::string::npos;
}

void CognitiveRouter::optimizeForAIWorkload(cPacket *packet, FlowInfo& flow)
{
    // AI workload optimizations
    
    // Track large flows (typical in AI workloads)
    if (flow.totalBytes > 10 * 1024 * 1024) { // 10MB threshold
        // This is a large flow, optimize for bandwidth
        EV << "Large AI flow detected, optimizing for bandwidth" << endl;
    }
    
    // Detect collective communication patterns
    std::string packetName = packet->getName();
    if (packetName.find("AllReduce") != std::string::npos) {
        // AllReduce pattern detected, optimize for low latency
        EV << "AllReduce pattern detected, optimizing for latency" << endl;
    }
}

std::string CognitiveRouter::extractFlowId(cPacket *packet)
{
    // Simple flow identification based on packet properties
    // In a real implementation, this would examine IP/TCP headers
    
    std::stringstream ss;
    ss << packet->getSenderModule()->getId() << "_" 
       << packet->getName() << "_"
       << (packet->getKind() % 1000); // Simple hash
    
    return ss.str();
}

void CognitiveRouter::updateRoutingDecision(cPacket *packet, int selectedPort)
{
    // Update path metrics
    updatePathMetrics(selectedPort, packet);
    
    // Update port utilization
    double alpha = 0.1;
    double instantUtil = (double)packet->getBitLength() / (100e9 * 0.001);
    portUtilization[selectedPort] = alpha * instantUtil + (1 - alpha) * portUtilization[selectedPort];
}

void CognitiveRouter::collectTelemetryData()
{
    // Collect and report advanced telemetry
    reportAdvancedMetrics();
}

void CognitiveRouter::reportAdvancedMetrics()
{
    for (int port = 0; port < gateSize("out"); port++) {
        EV << "Port " << port << " metrics: "
           << "Util=" << portUtilization[port] 
           << ", Congestion=" << pathMetrics[port].congestionLevel
           << ", Failures=" << pathMetrics[port].failureCount << endl;
    }
}

double CognitiveRouter::getPortUtilization(int port) const
{
    if (port >= 0 && port < (int)portUtilization.size()) {
        return portUtilization[port];
    }
    return 0.0;
}

double CognitiveRouter::getCongestionLevel(int port) const
{
    if (port >= 0 && port < (int)pathMetrics.size()) {
        return pathMetrics[port].congestionLevel;
    }
    return 0.0;
}

CognitiveRouter::PathMetrics CognitiveRouter::getPathMetrics(int port) const
{
    if (port >= 0 && port < (int)pathMetrics.size()) {
        return pathMetrics[port];
    }
    PathMetrics empty = {0, 0, 0, 0, 0};
    return empty;
}

void CognitiveRouter::finish()
{
    // Record final statistics
    for (int port = 0; port < gateSize("out"); port++) {
        std::stringstream ss;
        ss << "Port " << port << " Final Utilization";
        recordScalar(ss.str().c_str(), portUtilization[port]);
        
        ss.str("");
        ss << "Port " << port << " Failure Count";
        recordScalar(ss.str().c_str(), pathMetrics[port].failureCount);
    }
    
    recordScalar("Active Flows", activeFlows.size());
}

} // namespace tomahawk6