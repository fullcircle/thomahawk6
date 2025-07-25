//
// Simple Switch with configurable ports and capacity
//

#include <omnetpp.h>

using namespace omnetpp;

class SimpleSwitch : public cSimpleModule
{
  private:
    std::string portConfig;
    double capacity;
    int packetsForwarded;
    long totalBytes;
    cOutVector queueLengthVector;
    cOutVector utilizationVector;
    std::vector<cQueue> portQueues;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    void forwardPacket(cPacket *packet, int inGate);
};

Define_Module(SimpleSwitch);

void SimpleSwitch::initialize()
{
    portConfig = par("portConfig").stringValue();
    capacity = par("capacity");
    packetsForwarded = 0;
    totalBytes = 0;
    
    queueLengthVector.setName("Queue Length");
    utilizationVector.setName("Port Utilization");
    
    // Initialize port queues
    int numPorts = gateSize("in");
    portQueues.resize(numPorts);
    for (int i = 0; i < numPorts; i++) {
        portQueues[i].setName(("port_" + std::to_string(i) + "_queue").c_str());
    }
    
    EV << "Initializing SimpleSwitch: " << portConfig 
       << ", capacity: " << capacity << " bps" << endl;
}

void SimpleSwitch::handleMessage(cMessage *msg)
{
    cPacket *packet = check_and_cast<cPacket *>(msg);
    int inGate = packet->getArrivalGate()->getIndex();
    
    packetsForwarded++;
    totalBytes += packet->getByteLength();
    
    // Simple switching logic - round robin to other ports
    forwardPacket(packet, inGate);
    
    // Record queue statistics
    int totalQueueLength = 0;
    for (auto& queue : portQueues) {
        totalQueueLength += queue.getLength();
    }
    queueLengthVector.record(totalQueueLength);
    
    // Calculate utilization based on capacity
    double utilization = (totalBytes * 8.0) / (simTime().dbl() * capacity);
    utilizationVector.record(utilization);
}

void SimpleSwitch::forwardPacket(cPacket *packet, int inGate)
{
    int numPorts = gateSize("out");
    if (numPorts <= 1) {
        delete packet;
        return;
    }
    
    // Simple forwarding: send to next port (avoid sending back to input port)
    int outGate = (inGate + 1) % numPorts;
    
    // Add minimal switching delay
    double switchingDelay = packet->getByteLength() * 8.0 / capacity;
    sendDelayed(packet, switchingDelay, "out", outGate);
    
    EV << "Forwarding packet from port " << inGate 
       << " to port " << outGate << ", delay: " << switchingDelay << "s" << endl;
}

void SimpleSwitch::finish()
{
    recordScalar("Packets Forwarded", (double)packetsForwarded);
    recordScalar("Total Bytes", (double)totalBytes);
    // recordScalar("Port Configuration", portConfig.c_str()); // String scalars not supported
    recordScalar("Switching Capacity (bps)", capacity);
    recordScalar("Average Utilization", (totalBytes * 8.0) / (simTime().dbl() * capacity));
    
    EV << "SimpleSwitch finished: " << packetsForwarded 
       << " packets forwarded, " << totalBytes << " bytes" << endl;
}