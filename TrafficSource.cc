#include <omnetpp.h>

using namespace omnetpp;

class TrafficSource : public cSimpleModule
{
  private:
    cMessage *timer;
    int packetCount;
    
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
  public:
    TrafficSource();
    virtual ~TrafficSource();
};

Define_Module(TrafficSource);

TrafficSource::TrafficSource()
{
    timer = nullptr;
    packetCount = 0;
}

TrafficSource::~TrafficSource()
{
    cancelAndDelete(timer);
}

void TrafficSource::initialize()
{
    timer = new cMessage("generate");
    scheduleAt(simTime() + 0.001, timer);
    EV << "TrafficSource initialized" << endl;
}

void TrafficSource::handleMessage(cMessage *msg)
{
    if (msg == timer) {
        cPacket *packet = new cPacket("TomahawkPacket");
        packet->setByteLength(1000);
        packet->setKind(packetCount);
        
        send(packet, "out");
        packetCount++;
        
        EV << "Sent packet " << packetCount << endl;
        
        // Schedule next packet
        scheduleAt(simTime() + 0.001, timer);
    }
}

void TrafficSource::finish()
{
    EV << "TrafficSource sent " << packetCount << " packets" << endl;
    recordScalar("Packets Sent", packetCount);
}