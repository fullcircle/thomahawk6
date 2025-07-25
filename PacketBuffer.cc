#include "PacketBuffer.h"
#include "inet/common/packet/Packet.h"

namespace tomahawk6 {

Define_Module(PacketBuffer);

PacketBuffer::PacketBuffer()
{
    processingTimer = nullptr;
    processing = false;
    totalBufferUsed = 0;
    currentRRIndex = 0;
    lastAdaptationTime = 0;
}

PacketBuffer::~PacketBuffer()
{
    cancelAndDelete(processingTimer);
    
    // Clean up queued packets
    for (auto& queue : queues) {
        while (!queue.empty()) {
            delete queue.front();
            queue.pop();
        }
    }
}

void PacketBuffer::initialize()
{
    // Read parameters
    numQueues = par("numQueues");
    bufferSize = par("bufferSize");
    processingDelay = par("processingDelay");
    aiPriorityQueues = par("aiPriorityQueues");
    rocevSupport = par("rocevSupport");
    adaptiveBuffering = par("adaptiveBuffering");
    
    // Parse scheduling algorithm
    std::string schedAlg = par("schedulingAlgorithm").stdstringValue();
    if (schedAlg == "WRR") schedulingAlg = WEIGHTED_ROUND_ROBIN;
    else if (schedAlg == "SP") schedulingAlg = STRICT_PRIORITY;
    else if (schedAlg == "PQ") schedulingAlg = PRIORITY_QUEUEING;
    else schedulingAlg = AI_OPTIMIZED;
    
    // Initialize queues
    queues.resize(numQueues);
    queueTypes.resize(numQueues);
    queueWeights.resize(numQueues);
    queueSizes.resize(numQueues, 0);
    maxQueueSizes.resize(numQueues);
    queuePriorities.resize(numQueues, 1.0);
    
    // Configure queue types and weights
    for (int i = 0; i < numQueues; i++) {
        if (i < aiPriorityQueues) {
            queueTypes[i] = AI_PRIORITY_QUEUE;
            queueWeights[i] = 4;  // Higher weight for AI traffic
        } else if (rocevSupport && i == numQueues - 1) {
            queueTypes[i] = ROCEV_QUEUE;
            queueWeights[i] = 3;
        } else if (i == numQueues - 2) {
            queueTypes[i] = CONTROL_QUEUE;
            queueWeights[i] = 2;
        } else {
            queueTypes[i] = STANDARD_QUEUE;
            queueWeights[i] = 1;
        }
        
        // Distribute buffer space based on queue priority
        maxQueueSizes[i] = bufferSize / numQueues;
        if (queueTypes[i] == AI_PRIORITY_QUEUE) {
            maxQueueSizes[i] *= 2;  // Give AI queues more space
        }
    }
    
    // Initialize statistics
    queueLengthSignal = registerSignal("queueLength");
    bufferUtilizationSignal = registerSignal("bufferUtilization");
    packetDelaySignal = registerSignal("packetDelay");
    packetDropSignal = registerSignal("packetDrop");
    throughputSignal = registerSignal("throughput");
    
    // Create processing timer
    processingTimer = new cMessage("processQueue");
    
    EV << "PacketBuffer initialized with " << numQueues << " queues, "
       << bufferSize/1024/1024 << " MB total buffer" << endl;
}

void PacketBuffer::handleMessage(cMessage *msg)
{
    if (msg == processingTimer) {
        // Process queued packets
        if (!processing) {
            cPacket *packet = dequeuePacket();
            if (packet != nullptr) {
                processing = true;
                
                // Calculate packet delay
                simtime_t delay = simTime() - packet->getCreationTime();
                emit(packetDelaySignal, delay);
                
                // Send packet out with processing delay
                sendDelayed(packet, processingDelay, "out", 0);
                
                // Schedule next processing cycle
                scheduleAt(simTime() + processingDelay, processingTimer);
                
                emit(throughputSignal, packet->getBitLength());
            } else {
                processing = false;
            }
        }
        return;
    }
    
    // Incoming packet
    cPacket *packet = check_and_cast<cPacket*>(msg);
    
    // Classify packet and determine queue
    int queueIndex = classifyPacket(packet);
    
    // Try to enqueue
    if (!enqueuePacket(packet, queueIndex)) {
        // Buffer full, drop packet
        EV << "Packet dropped due to buffer overflow in queue " << queueIndex << endl;
        emit(packetDropSignal, 1);
        delete packet;
        return;
    }
    
    // Start processing if not already active
    if (!processing) {
        scheduleAt(simTime(), processingTimer);
    }
    
    // Adaptive buffering
    if (adaptiveBuffering && (simTime() - lastAdaptationTime) > 0.001) {
        adaptBufferAllocation();
        lastAdaptationTime = simTime();
    }
    
    updateBufferStatistics();
}

bool PacketBuffer::enqueuePacket(cPacket *packet, int queueIndex)
{
    if (!hasSpaceInBuffer(packet)) {
        return false;
    }
    
    long packetSize = packet->getByteLength();
    
    // Check queue-specific limits
    if (queueSizes[queueIndex] + packetSize > maxQueueSizes[queueIndex]) {
        // Try to steal space from lower priority queues if adaptive
        if (adaptiveBuffering && queueTypes[queueIndex] == AI_PRIORITY_QUEUE) {
            // Implement adaptive buffer stealing logic
            for (int i = numQueues - 1; i >= 0; i--) {
                if (queueTypes[i] == STANDARD_QUEUE && maxQueueSizes[i] > bufferSize / numQueues / 2) {
                    maxQueueSizes[i] -= packetSize;
                    maxQueueSizes[queueIndex] += packetSize;
                    break;
                }
            }
        } else {
            return false;
        }
    }
    
    // Enqueue packet
    queues[queueIndex].push(packet);
    queueSizes[queueIndex] += packetSize;
    totalBufferUsed += packetSize;
    
    // Handle RoCEv2 specific processing
    if (rocevSupport && queueTypes[queueIndex] == ROCEV_QUEUE) {
        handleRoCEv2Packet(packet);
    }
    
    EV << "Packet enqueued in queue " << queueIndex 
       << ", queue size: " << queues[queueIndex].size() << endl;
    
    return true;
}

cPacket* PacketBuffer::dequeuePacket()
{
    int selectedQueue = selectNextQueue();
    if (selectedQueue == -1) {
        return nullptr;
    }
    
    cPacket *packet = queues[selectedQueue].front();
    queues[selectedQueue].pop();
    
    long packetSize = packet->getByteLength();
    queueSizes[selectedQueue] -= packetSize;
    totalBufferUsed -= packetSize;
    
    EV << "Packet dequeued from queue " << selectedQueue << endl;
    
    return packet;
}

int PacketBuffer::selectNextQueue()
{
    switch (schedulingAlg) {
        case WEIGHTED_ROUND_ROBIN:
            return weightedRoundRobin();
        case STRICT_PRIORITY:
            return strictPriority();
        case AI_OPTIMIZED:
            return aiOptimizedScheduling();
        default:
            return strictPriority();
    }
}

int PacketBuffer::classifyPacket(cPacket *packet)
{
    // Simple classification based on packet properties
    // In a real implementation, this would examine packet headers
    
    std::string packetName = packet->getName();
    
    // AI/ML traffic classification
    if (packetName.find("AllReduce") != std::string::npos ||
        packetName.find("AllGather") != std::string::npos ||
        packetName.find("AI") != std::string::npos) {
        return 0;  // First AI priority queue
    }
    
    // RoCEv2 traffic
    if (rocevSupport && packetName.find("RoCE") != std::string::npos) {
        return numQueues - 1;  // RoCEv2 queue
    }
    
    // Control traffic
    if (packetName.find("Control") != std::string::npos) {
        return numQueues - 2;  // Control queue
    }
    
    // Default to standard queue
    return aiPriorityQueues + (uniform(0, numQueues - aiPriorityQueues - 2));
}

int PacketBuffer::weightedRoundRobin()
{
    // Weighted round-robin implementation
    for (int attempts = 0; attempts < numQueues; attempts++) {
        currentRRIndex = (currentRRIndex + 1) % numQueues;
        
        if (!queues[currentRRIndex].empty()) {
            // Check if this queue should be served based on weight
            if (uniform(0, queueWeights[currentRRIndex]) > 0.5) {
                return currentRRIndex;
            }
        }
    }
    return -1;
}

int PacketBuffer::strictPriority()
{
    // Serve highest priority non-empty queue first
    for (int i = 0; i < numQueues; i++) {
        if (!queues[i].empty()) {
            return i;
        }
    }
    return -1;
}

int PacketBuffer::aiOptimizedScheduling()
{
    // AI-optimized scheduling considers queue priorities and traffic patterns
    double maxPriority = 0;
    int selectedQueue = -1;
    
    for (int i = 0; i < numQueues; i++) {
        if (!queues[i].empty()) {
            double priority = queuePriorities[i] * queueWeights[i];
            
            // Boost priority for AI queues under heavy load
            if (queueTypes[i] == AI_PRIORITY_QUEUE && queues[i].size() > 10) {
                priority *= 2.0;
            }
            
            if (priority > maxPriority) {
                maxPriority = priority;
                selectedQueue = i;
            }
        }
    }
    
    return selectedQueue;
}

bool PacketBuffer::hasSpaceInBuffer(cPacket *packet)
{
    return totalBufferUsed + packet->getByteLength() <= bufferSize;
}

void PacketBuffer::adaptBufferAllocation()
{
    // Adaptive buffer allocation based on queue utilization
    for (int i = 0; i < numQueues; i++) {
        double utilization = (double)queueSizes[i] / maxQueueSizes[i];
        
        // Adjust queue priorities based on utilization
        if (utilization > 0.8 && queueTypes[i] == AI_PRIORITY_QUEUE) {
            queuePriorities[i] = std::min(2.0, queuePriorities[i] * 1.1);
        } else if (utilization < 0.2) {
            queuePriorities[i] = std::max(0.5, queuePriorities[i] * 0.9);
        }
    }
}

void PacketBuffer::handleRoCEv2Packet(cPacket *packet)
{
    // RoCEv2 specific handling - implement RDMA semantics
    // This could include priority boosting, specific buffer management, etc.
    EV << "Handling RoCEv2 packet: " << packet->getName() << endl;
}

void PacketBuffer::updateBufferStatistics()
{
    emit(bufferUtilizationSignal, getBufferUtilization());
    
    for (int i = 0; i < numQueues; i++) {
        emit(queueLengthSignal, queues[i].size());
    }
}

int PacketBuffer::getQueueLength(int queueIndex) const
{
    if (queueIndex >= 0 && queueIndex < numQueues) {
        return queues[queueIndex].size();
    }
    return 0;
}

void PacketBuffer::finish()
{
    // Record final statistics
    recordScalar("Final Buffer Utilization", getBufferUtilization());
    recordScalar("Total Packets Processed", throughputSignal);
    
    for (int i = 0; i < numQueues; i++) {
        std::stringstream ss;
        ss << "Queue " << i << " Final Length";
        recordScalar(ss.str().c_str(), queues[i].size());
    }
}

} // namespace tomahawk6