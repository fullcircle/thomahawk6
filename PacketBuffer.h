#ifndef __TOMAHAWK6_PACKETBUFFER_H_
#define __TOMAHAWK6_PACKETBUFFER_H_

#include <omnetpp.h>
#include <queue>
#include <vector>
#include "inet/common/INETDefs.h"
#include "inet/common/packet/Packet.h"

using namespace omnetpp;
using namespace inet;

namespace tomahawk6 {

/**
 * Multi-level packet buffer with AI/ML optimizations
 * Supports various scheduling algorithms and adaptive buffering
 */
class INET_API PacketBuffer : public cSimpleModule
{
  public:
    enum SchedulingAlgorithm {
        WEIGHTED_ROUND_ROBIN,
        STRICT_PRIORITY,
        PRIORITY_QUEUEING,
        AI_OPTIMIZED
    };
    
    enum QueueType {
        STANDARD_QUEUE,
        AI_PRIORITY_QUEUE,
        ROCEV_QUEUE,
        CONTROL_QUEUE
    };

  private:
    // Configuration
    int numQueues;
    long bufferSize;
    SchedulingAlgorithm schedulingAlg;
    simtime_t processingDelay;
    
    // AI/ML specific parameters
    int aiPriorityQueues;
    bool rocevSupport;
    bool adaptiveBuffering;
    
    // Queue structures
    std::vector<std::queue<cPacket*>> queues;
    std::vector<QueueType> queueTypes;
    std::vector<int> queueWeights;
    std::vector<long> queueSizes;
    std::vector<long> maxQueueSizes;
    
    long totalBufferUsed;
    int currentRRIndex;  // For round-robin scheduling
    
    // Timers and state
    cMessage *processingTimer;
    bool processing;
    
    // Statistics
    simsignal_t queueLengthSignal;
    simsignal_t bufferUtilizationSignal;
    simsignal_t packetDelaySignal;
    simsignal_t packetDropSignal;
    simsignal_t throughputSignal;
    
    // Adaptive buffering state
    std::vector<double> queuePriorities;
    simtime_t lastAdaptationTime;
    
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
    // Buffer management
    virtual bool enqueuePacket(cPacket *packet, int queueIndex);
    virtual cPacket* dequeuePacket();
    virtual int selectNextQueue();
    virtual bool hasSpaceInBuffer(cPacket *packet);
    virtual void updateBufferStatistics();
    
    // AI optimizations
    virtual int classifyPacket(cPacket *packet);
    virtual void adaptBufferAllocation();
    virtual void handleRoCEv2Packet(cPacket *packet);
    
    // Scheduling algorithms
    virtual int weightedRoundRobin();
    virtual int strictPriority();
    virtual int aiOptimizedScheduling();
    
  public:
    PacketBuffer();
    virtual ~PacketBuffer();
    
    // Public interface
    long getTotalBufferUsed() const { return totalBufferUsed; }
    double getBufferUtilization() const { return (double)totalBufferUsed / bufferSize; }
    int getQueueLength(int queueIndex) const;
};

} // namespace tomahawk6

#endif