#ifndef __TOMAHAWK6_AITRAFFICGENERATOR_H_
#define __TOMAHAWK6_AITRAFFICGENERATOR_H_

#include <omnetpp.h>
#include <vector>
#include <string>
#include "inet/common/INETDefs.h"
#include "inet/common/packet/Packet.h"

using namespace omnetpp;
using namespace inet;

namespace tomahawk6 {

/**
 * AI Workload Traffic Generator
 * Generates realistic AI/ML traffic patterns including AllReduce, AllGather, etc.
 */
class INET_API AITrafficGenerator : public cSimpleModule
{
  public:
    enum AIWorkloadType {
        ALL_REDUCE,
        ALL_GATHER,
        REDUCE_SCATTER,
        POINT_TO_POINT,
        BROADCAST,
        ALL_TO_ALL
    };
    
    struct CollectiveOperation {
        AIWorkloadType type;
        int participantCount;
        long dataSize;
        simtime_t startTime;
        simtime_t duration;
        std::vector<int> participants;
    };

  private:
    // Configuration parameters
    AIWorkloadType workloadType;
    double trafficIntensity;
    long burstSize;
    simtime_t burstInterval;
    bool rocevProtocol;
    long flowSize;
    
    // AI-specific parameters
    int tensorSize;
    int batchSize;
    int numGPUs;
    double computeToCommRatio;
    
    // State tracking
    std::vector<CollectiveOperation> activeOperations;
    long totalBytesSent;
    int packetsSent;
    
    // Timers
    cMessage *burstTimer;
    cMessage *collectiveTimer;
    std::vector<cMessage*> operationTimers;
    
    // Statistics
    simsignal_t generatedTrafficSignal;
    simsignal_t burstSizeSignal;
    simsignal_t collectiveLatencySignal;
    
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
    // Traffic generation functions
    virtual void generateBurst();
    virtual void startCollectiveOperation(AIWorkloadType type);
    virtual void generateAllReduceTraffic();
    virtual void generateAllGatherTraffic();
    virtual void generateReduceScatterTraffic();
    virtual void generateP2PTraffic();
    
    // Packet creation
    virtual cPacket* createAIPacket(const std::string& name, long size, AIWorkloadType type);
    virtual void addRoCEHeaders(cPacket* packet);
    virtual void addCollectiveMetadata(cPacket* packet, const CollectiveOperation& op);
    
    // Workload modeling
    virtual simtime_t calculateCollectiveDuration(AIWorkloadType type, int participants, long dataSize);
    virtual std::vector<int> selectParticipants(int count);
    virtual long calculateMessageSize(AIWorkloadType type);
    
    // AI workload patterns
    virtual void modelTrainingIteration();
    virtual void modelInferenceWorkload();
    virtual void modelParameterServerPattern();
    
  public:
    AITrafficGenerator();
    virtual ~AITrafficGenerator();
    
    // Public interface
    long getTotalBytesSent() const { return totalBytesSent; }
    int getPacketsSent() const { return packetsSent; }
    std::string getWorkloadTypeString() const;
};

} // namespace tomahawk6

#endif