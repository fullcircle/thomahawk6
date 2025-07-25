#include "AITrafficGenerator.h"
#include "inet/common/packet/Packet.h"
#include <sstream>
#include <algorithm>

namespace tomahawk6 {

Define_Module(AITrafficGenerator);

AITrafficGenerator::AITrafficGenerator()
{
    burstTimer = nullptr;
    collectiveTimer = nullptr;
    totalBytesSent = 0;
    packetsSent = 0;
}

AITrafficGenerator::~AITrafficGenerator()
{
    cancelAndDelete(burstTimer);
    cancelAndDelete(collectiveTimer);
    
    for (auto timer : operationTimers) {
        cancelAndDelete(timer);
    }
}

void AITrafficGenerator::initialize()
{
    // Read parameters
    std::string workloadStr = par("workloadType").stdstringValue();
    if (workloadStr == "AllReduce") workloadType = ALL_REDUCE;
    else if (workloadStr == "AllGather") workloadType = ALL_GATHER;
    else if (workloadStr == "ReduceScatter") workloadType = REDUCE_SCATTER;
    else if (workloadStr == "P2P") workloadType = POINT_TO_POINT;
    else if (workloadStr == "Broadcast") workloadType = BROADCAST;
    else if (workloadStr == "AllToAll") workloadType = ALL_TO_ALL;
    else workloadType = ALL_REDUCE;
    
    trafficIntensity = par("trafficIntensity");
    burstSize = par("burstSize");
    burstInterval = par("burstInterval");
    rocevProtocol = par("rocevProtocol");
    flowSize = par("flowSize");
    
    // AI-specific parameters (with defaults)
    tensorSize = par("tensorSize");
    batchSize = par("batchSize");
    numGPUs = par("numGPUs");
    computeToCommRatio = par("computeToCommRatio");
    
    // Initialize statistics
    generatedTrafficSignal = registerSignal("generatedTraffic");
    burstSizeSignal = registerSignal("burstSize");
    collectiveLatencySignal = registerSignal("collectiveLatency");
    
    // Create timers
    burstTimer = new cMessage("burstTimer");
    collectiveTimer = new cMessage("collectiveTimer");
    
    // Schedule initial traffic generation
    scheduleAt(simTime() + exponential(burstInterval.dbl()), burstTimer);
    
    // Schedule collective operations
    scheduleAt(simTime() + exponential(0.01), collectiveTimer); // 10ms average interval
    
    EV << "AITrafficGenerator initialized: " << getWorkloadTypeString()
       << ", Intensity: " << trafficIntensity
       << ", RoCEv2: " << (rocevProtocol ? "enabled" : "disabled") << endl;
}

void AITrafficGenerator::handleMessage(cMessage *msg)
{
    if (msg == burstTimer) {
        generateBurst();
        
        // Schedule next burst
        simtime_t nextBurst = exponential(burstInterval.dbl() / trafficIntensity);
        scheduleAt(simTime() + nextBurst, burstTimer);
        return;
    }
    
    if (msg == collectiveTimer) {
        // Start a collective operation
        startCollectiveOperation(workloadType);
        
        // Schedule next collective operation
        simtime_t nextCollective = exponential(0.01 / trafficIntensity); // Scale with intensity
        scheduleAt(simTime() + nextCollective, collectiveTimer);
        return;
    }
    
    // Check if it's an operation completion timer
    auto timerIt = std::find(operationTimers.begin(), operationTimers.end(), msg);
    if (timerIt != operationTimers.end()) {
        // Complete the collective operation
        EV << "Collective operation completed at " << simTime() << endl;
        operationTimers.erase(timerIt);
        delete msg;
        return;
    }
    
    // Handle feedback messages
    if (msg->arrivedOn("feedback")) {
        EV << "Received feedback: " << msg->getName() << endl;
        delete msg;
        return;
    }
}

void AITrafficGenerator::generateBurst()
{
    int packetsInBurst = std::max(1L, burstSize / 1500); // Assume 1500 byte packets
    
    for (int i = 0; i < packetsInBurst; i++) {
        cPacket *packet = createAIPacket("AITraffic", 1500, workloadType);
        
        if (rocevProtocol) {
            addRoCEHeaders(packet);
        }
        
        // Add some jitter to packet timing within burst
        simtime_t sendTime = simTime() + uniform(0, 0.001); // Up to 1ms jitter
        sendDelayed(packet, sendTime - simTime(), "out");
        
        totalBytesSent += packet->getByteLength();
        packetsSent++;
        
        emit(generatedTrafficSignal, packet->getBitLength());
    }
    
    emit(burstSizeSignal, packetsInBurst);
    EV << "Generated burst of " << packetsInBurst << " packets" << endl;
}

void AITrafficGenerator::startCollectiveOperation(AIWorkloadType type)
{
    CollectiveOperation op;
    op.type = type;
    op.participantCount = uniform(4, numGPUs); // Random number of participants
    op.dataSize = calculateMessageSize(type);
    op.startTime = simTime();
    op.participants = selectParticipants(op.participantCount);
    op.duration = calculateCollectiveDuration(type, op.participantCount, op.dataSize);
    
    activeOperations.push_back(op);
    
    // Generate traffic for this collective operation
    switch (type) {
        case ALL_REDUCE:
            generateAllReduceTraffic();
            break;
        case ALL_GATHER:
            generateAllGatherTraffic();
            break;
        case REDUCE_SCATTER:
            generateReduceScatterTraffic();
            break;
        case POINT_TO_POINT:
            generateP2PTraffic();
            break;
        default:
            generateAllReduceTraffic(); // Default case
            break;
    }
    
    // Schedule operation completion
    cMessage *completionTimer = new cMessage("operationComplete");
    operationTimers.push_back(completionTimer);
    scheduleAt(simTime() + op.duration, completionTimer);
    
    EV << "Started " << getWorkloadTypeString() << " operation with " 
       << op.participantCount << " participants, " 
       << op.dataSize/1024/1024 << " MB data" << endl;
}

void AITrafficGenerator::generateAllReduceTraffic()
{
    // AllReduce: each participant sends to all others, then receives reduced result
    // Phase 1: Reduce-scatter (each node gets partial results)
    // Phase 2: All-gather (distribute final results)
    
    long messageSize = tensorSize / numGPUs; // Divide tensor among participants
    
    // Generate reduce-scatter phase traffic
    for (int round = 0; round < numGPUs - 1; round++) {
        cPacket *packet = createAIPacket("AllReduce_RS", messageSize, ALL_REDUCE);
        packet->addPar("phase") = "reduce_scatter";
        packet->addPar("round") = round;
        
        if (rocevProtocol) {
            addRoCEHeaders(packet);
        }
        
        simtime_t sendDelay = round * 0.001; // 1ms between rounds
        sendDelayed(packet, sendDelay, "out");
        
        totalBytesSent += packet->getByteLength();
        packetsSent++;
    }
    
    // Generate all-gather phase traffic
    for (int round = 0; round < numGPUs - 1; round++) {
        cPacket *packet = createAIPacket("AllReduce_AG", messageSize, ALL_REDUCE);
        packet->addPar("phase") = "all_gather";
        packet->addPar("round") = round;
        
        if (rocevProtocol) {
            addRoCEHeaders(packet);
        }
        
        simtime_t sendDelay = (numGPUs - 1) * 0.001 + round * 0.001;
        sendDelayed(packet, sendDelay, "out");
        
        totalBytesSent += packet->getByteLength();
        packetsSent++;
    }
}

void AITrafficGenerator::generateAllGatherTraffic()
{
    // AllGather: each participant broadcasts its data to all others
    long messageSize = tensorSize / numGPUs;
    
    for (int participant = 0; participant < numGPUs; participant++) {
        cPacket *packet = createAIPacket("AllGather", messageSize, ALL_GATHER);
        packet->addPar("source") = participant;
        
        if (rocevProtocol) {
            addRoCEHeaders(packet);
        }
        
        simtime_t sendDelay = participant * 0.0005; // 0.5ms stagger
        sendDelayed(packet, sendDelay, "out");
        
        totalBytesSent += packet->getByteLength();
        packetsSent++;
    }
}

void AITrafficGenerator::generateReduceScatterTraffic()
{
    // ReduceScatter: data is reduced and scattered among participants
    long totalData = tensorSize;
    long messageSize = totalData / numGPUs;
    
    for (int round = 0; round < numGPUs - 1; round++) {
        cPacket *packet = createAIPacket("ReduceScatter", messageSize, REDUCE_SCATTER);
        packet->addPar("round") = round;
        
        if (rocevProtocol) {
            addRoCEHeaders(packet);
        }
        
        simtime_t sendDelay = round * 0.001;
        sendDelayed(packet, sendDelay, "out");
        
        totalBytesSent += packet->getByteLength();
        packetsSent++;
    }
}

void AITrafficGenerator::generateP2PTraffic()
{
    // Point-to-point communication
    long messageSize = uniform(1024, flowSize); // Variable message size
    
    cPacket *packet = createAIPacket("P2P", messageSize, POINT_TO_POINT);
    
    if (rocevProtocol) {
        addRoCEHeaders(packet);
    }
    
    send(packet, "out");
    
    totalBytesSent += packet->getByteLength();
    packetsSent++;
}

cPacket* AITrafficGenerator::createAIPacket(const std::string& name, long size, AIWorkloadType type)
{
    std::stringstream packetName;
    packetName << name << "_" << packetsSent;
    
    cPacket *packet = new cPacket(packetName.str().c_str());
    packet->setByteLength(size);
    packet->setKind(type);
    packet->setTimestamp(simTime());
    
    // Add AI-specific metadata
    packet->addPar("aiWorkload") = true;
    packet->addPar("tensorSize") = tensorSize;
    packet->addPar("batchSize") = batchSize;
    packet->addPar("workloadType") = (int)type;
    
    return packet;
}

void AITrafficGenerator::addRoCEHeaders(cPacket* packet)
{
    // Simulate RoCEv2 header overhead
    long originalSize = packet->getByteLength();
    packet->setByteLength(originalSize + 42); // RoCEv2 header size
    
    // Add RoCEv2 specific parameters
    packet->addPar("roce") = true;
    packet->addPar("queuePair") = uniform(1, 1000);
    packet->addPar("packetSeqNum") = packetsSent;
    
    EV << "Added RoCEv2 headers to packet " << packet->getName() << endl;
}

void AITrafficGenerator::addCollectiveMetadata(cPacket* packet, const CollectiveOperation& op)
{
    packet->addPar("collectiveType") = (int)op.type;
    packet->addPar("participantCount") = op.participantCount;
    packet->addPar("operationId") = activeOperations.size() - 1;
}

simtime_t AITrafficGenerator::calculateCollectiveDuration(AIWorkloadType type, int participants, long dataSize)
{
    // Model collective operation duration based on algorithm complexity
    double baseLatency = 0.001; // 1ms base
    double bandwidthFactor = (double)dataSize / (100e9); // Assume 100Gbps
    
    switch (type) {
        case ALL_REDUCE:
            // O(log P) algorithm with 2 phases
            return baseLatency * log2(participants) * 2 + bandwidthFactor;
        case ALL_GATHER:
            // O(P) algorithm
            return baseLatency * participants + bandwidthFactor;
        case REDUCE_SCATTER:
            // O(log P) algorithm
            return baseLatency * log2(participants) + bandwidthFactor;
        case POINT_TO_POINT:
            return baseLatency + bandwidthFactor;
        default:
            return baseLatency + bandwidthFactor;
    }
}

std::vector<int> AITrafficGenerator::selectParticipants(int count)
{
    std::vector<int> participants;
    for (int i = 0; i < count; i++) {
        participants.push_back(i);
    }
    return participants;
}

long AITrafficGenerator::calculateMessageSize(AIWorkloadType type)
{
    switch (type) {
        case ALL_REDUCE:
            return tensorSize; // Full tensor size
        case ALL_GATHER:
            return tensorSize / numGPUs; // Per-GPU chunk
        case REDUCE_SCATTER:
            return tensorSize; // Full tensor, then scattered
        case POINT_TO_POINT:
            return uniform(1024, flowSize);
        default:
            return tensorSize;
    }
}

void AITrafficGenerator::modelTrainingIteration()
{
    // Model a complete training iteration
    // 1. Forward pass (minimal communication)
    // 2. Backward pass with gradient synchronization
    
    EV << "Modeling training iteration" << endl;
    
    // Generate gradient synchronization traffic (AllReduce)
    startCollectiveOperation(ALL_REDUCE);
}

void AITrafficGenerator::modelInferenceWorkload()
{
    // Model inference workload with different communication patterns
    EV << "Modeling inference workload" << endl;
    
    // Inference typically has more P2P communication
    for (int i = 0; i < uniform(1, 5); i++) {
        generateP2PTraffic();
    }
}

void AITrafficGenerator::modelParameterServerPattern()
{
    // Model parameter server communication pattern
    EV << "Modeling parameter server pattern" << endl;
    
    // Workers send gradients to parameter server
    for (int worker = 0; worker < numGPUs; worker++) {
        cPacket *gradientPacket = createAIPacket("Gradient", tensorSize / numGPUs, POINT_TO_POINT);
        gradientPacket->addPar("destination") = "ParameterServer";
        
        sendDelayed(gradientPacket, worker * 0.0001, "out"); // 0.1ms stagger
        
        totalBytesSent += gradientPacket->getByteLength();
        packetsSent++;
    }
    
    // Parameter server sends updated parameters back
    for (int worker = 0; worker < numGPUs; worker++) {
        cPacket *paramPacket = createAIPacket("Parameters", tensorSize / numGPUs, POINT_TO_POINT);
        paramPacket->addPar("source") = "ParameterServer";
        
        simtime_t sendDelay = 0.001 + worker * 0.0001; // After gradient collection
        sendDelayed(paramPacket, sendDelay, "out");
        
        totalBytesSent += paramPacket->getByteLength();
        packetsSent++;
    }
}

std::string AITrafficGenerator::getWorkloadTypeString() const
{
    switch (workloadType) {
        case ALL_REDUCE: return "AllReduce";
        case ALL_GATHER: return "AllGather";
        case REDUCE_SCATTER: return "ReduceScatter";
        case POINT_TO_POINT: return "P2P";
        case BROADCAST: return "Broadcast";
        case ALL_TO_ALL: return "AllToAll";
        default: return "Unknown";
    }
}

void AITrafficGenerator::finish()
{
    // Record final statistics
    recordScalar("Total Bytes Sent", totalBytesSent);
    recordScalar("Packets Sent", packetsSent);
    recordScalar("Average Packet Size", packetsSent > 0 ? (double)totalBytesSent / packetsSent : 0);
    recordScalar("Active Operations", activeOperations.size());
    
    // Calculate throughput
    simtime_t duration = simTime();
    if (duration > 0) {
        double throughputBps = (double)(totalBytesSent * 8) / duration.dbl();
        recordScalar("Average Throughput (bps)", throughputBps);
    }
    
    EV << "AITrafficGenerator finished: " << totalBytesSent/1024/1024 << " MB sent, " 
       << packetsSent << " packets" << endl;
}

} // namespace tomahawk6