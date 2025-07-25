//
// Advanced Sink with AI workload analysis
//

#include <omnetpp.h>

using namespace omnetpp;

class AdvancedSink : public cSimpleModule
{
  private:
    int packetsReceived;
    long totalBytes;
    std::map<std::string, int> workloadCounts;
    simtime_t lastPacketTime;
    cOutVector throughputVector;
    cOutVector latencyVector;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
};

Define_Module(AdvancedSink);

void AdvancedSink::initialize()
{
    packetsReceived = 0;
    totalBytes = 0;
    lastPacketTime = 0;
    
    throughputVector.setName("Throughput");
    latencyVector.setName("Latency");
    
    EV << "Initializing AdvancedSink for AI workload analysis" << endl;
}

void AdvancedSink::handleMessage(cMessage *msg)
{
    cPacket *packet = check_and_cast<cPacket *>(msg);
    
    packetsReceived++;
    totalBytes += packet->getByteLength();
    
    // Analyze AI workload characteristics
    if (packet->hasPar("workloadType")) {
        std::string workloadType = packet->par("workloadType").stringValue();
        workloadCounts[workloadType]++;
        
        EV << "Received " << workloadType << " packet, size: " 
           << packet->getByteLength() << " bytes" << endl;
    }
    
    // Calculate and record latency
    simtime_t latency = simTime() - packet->getCreationTime();
    latencyVector.record(latency);
    
    // Calculate throughput
    if (lastPacketTime > 0) {
        double timeDiff = (simTime() - lastPacketTime).dbl();
        if (timeDiff > 0) {
            double instantThroughput = packet->getByteLength() / timeDiff;
            throughputVector.record(instantThroughput);
        }
    }
    lastPacketTime = simTime();
    
    delete msg;
}

void AdvancedSink::finish()
{
    recordScalar("AI Packets Received", (double)packetsReceived);
    recordScalar("Total Bytes", (double)totalBytes);
    recordScalar("Average Throughput (bytes/sec)", totalBytes / simTime().dbl());
    
    // Record per-workload statistics
    for (auto& pair : workloadCounts) {
        std::string statName = "Packets_" + pair.first;
        recordScalar(statName.c_str(), (double)pair.second);
    }
    
    EV << "AdvancedSink finished: " << packetsReceived 
       << " packets, " << totalBytes << " bytes" << endl;
}