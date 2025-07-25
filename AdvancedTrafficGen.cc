//
// Advanced Traffic Generator with AI workload simulation
//

#include <omnetpp.h>

using namespace omnetpp;

class AdvancedTrafficGen : public cSimpleModule
{
  private:
    cMessage *timer;
    int packetCount;
    std::string workloadType;
    double trafficIntensity;
    long tensorSize;
    int numGPUs;
    bool rocevProtocol;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
};

Define_Module(AdvancedTrafficGen);

void AdvancedTrafficGen::initialize()
{
    timer = new cMessage("timer");
    packetCount = 0;
    
    // Read parameters
    workloadType = par("workloadType").stringValue();
    trafficIntensity = par("trafficIntensity");
    tensorSize = par("tensorSize");
    numGPUs = par("numGPUs");
    rocevProtocol = par("rocevProtocol");
    
    EV << "Initializing AdvancedTrafficGen: " << workloadType 
       << " workload, " << numGPUs << " GPUs, " 
       << tensorSize << " B tensors" << endl;
    
    scheduleAt(simTime() + uniform(0.001, 0.01), timer);
}

void AdvancedTrafficGen::handleMessage(cMessage *msg)
{
    if (msg == timer) {
        // Generate AI workload traffic
        cPacket *packet = new cPacket(("AI_" + workloadType + "_Packet").c_str());
        
        // Set packet size based on workload type and tensor size
        int packetSize = 1000; // Base size
        if (workloadType == "AllReduce") {
            packetSize = tensorSize / numGPUs; // Distributed across GPUs
        } else if (workloadType == "AllGather") {
            packetSize = tensorSize / (numGPUs * 2); // Gathering phase
        } else if (workloadType == "P2P") {
            packetSize = tensorSize; // Point-to-point transfer
        }
        
        packet->setByteLength(packetSize);
        packet->setKind(packetCount);
        
        // Add AI workload metadata
        packet->addPar("workloadType") = workloadType.c_str();
        packet->addPar("tensorSize") = tensorSize;
        packet->addPar("rocevProtocol") = rocevProtocol;
        
        send(packet, "out");
        packetCount++;
        
        // Schedule next packet based on traffic intensity
        double nextInterval = exponential(0.001 / trafficIntensity);
        scheduleAt(simTime() + nextInterval, timer);
    }
}

void AdvancedTrafficGen::finish()
{
    recordScalar("AI Packets Sent", (double)packetCount);
    // recordScalar("Workload Type", workloadType.c_str()); // String scalars not supported
    recordScalar("Tensor Size (B)", (double)tensorSize);
    recordScalar("Number of GPUs", (double)numGPUs);
    recordScalar("Traffic Intensity", trafficIntensity);
}