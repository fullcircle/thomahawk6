#include "SerDesCore.h"

namespace tomahawk6 {

Define_Module(SerDesCore);

SerDesCore::SerDesCore()
{
    endTransmissionTimer = nullptr;
    busy = false;
}

SerDesCore::~SerDesCore()
{
    cancelAndDelete(endTransmissionTimer);
}

void SerDesCore::initialize()
{
    // Read parameters
    serdesType = par("serdesType").stdstringValue();
    dataRate = par("dataRate");
    latency = par("latency");
    
    // Initialize state
    busy = false;
    transmissionStartTime = 0;
    
    // Initialize statistics
    throughputSignal = registerSignal("throughput");
    utilizationSignal = registerSignal("utilization");
    
    // Create timer for transmission end
    endTransmissionTimer = new cMessage("endTransmission");
    
    EV << "SerDesCore initialized: " << serdesType 
       << " at " << dataRate/1e9 << " Gbps" << endl;
}

void SerDesCore::handleMessage(cMessage *msg)
{
    if (msg == endTransmissionTimer) {
        endTransmission();
        return;
    }
    
    cPacket *packet = check_and_cast<cPacket*>(msg);
    
    if (busy) {
        // SerDes is busy, this would typically cause backpressure
        // For now, we'll queue it briefly or drop it
        EV << "SerDes busy, packet from " << packet->getSenderModule()->getFullName() 
           << " experiencing backpressure" << endl;
        
        // In a real implementation, this would trigger flow control
        delete packet;
        return;
    }
    
    startTransmission(packet);
}

simtime_t SerDesCore::calculateTransmissionTime(cPacket *packet)
{
    // Calculate transmission time based on packet size and data rate
    long packetBits = packet->getBitLength();
    if (packetBits == 0) {
        packetBits = packet->getByteLength() * 8;
    }
    
    simtime_t transmissionTime = packetBits / dataRate;
    return transmissionTime + latency;
}

void SerDesCore::startTransmission(cPacket *packet)
{
    busy = true;
    transmissionStartTime = simTime();
    
    simtime_t transmissionTime = calculateTransmissionTime(packet);
    
    // Schedule end of transmission
    scheduleAt(simTime() + transmissionTime, endTransmissionTimer);
    
    // Send packet with appropriate delay
    sendDelayed(packet, transmissionTime, "out");
    
    // Update statistics
    emit(throughputSignal, packet->getBitLength());
    
    EV << "SerDes " << serdesType << " started transmission of " 
       << packet->getByteLength() << " bytes, duration: " 
       << transmissionTime << "s" << endl;
}

void SerDesCore::endTransmission()
{
    busy = false;
    
    // Calculate utilization
    simtime_t utilizationPeriod = simTime() - transmissionStartTime;
    if (utilizationPeriod > 0) {
        double utilization = (transmissionStartTime > 0) ? 1.0 : 0.0;
        emit(utilizationSignal, utilization);
    }
    
    EV << "SerDes transmission completed at " << simTime() << endl;
}

void SerDesCore::finish()
{
    // Record final statistics
    recordScalar("Final Utilization", busy ? 1.0 : 0.0);
}

} // namespace tomahawk6