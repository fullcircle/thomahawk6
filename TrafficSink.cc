#include <omnetpp.h>

using namespace omnetpp;

class TrafficSink : public cSimpleModule
{
  private:
    int packetsReceived;
    long totalBytes;
    
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
  public:
    TrafficSink();
    virtual ~TrafficSink();
};

Define_Module(TrafficSink);

TrafficSink::TrafficSink()
{
    packetsReceived = 0;
    totalBytes = 0;
}

TrafficSink::~TrafficSink()
{
}

void TrafficSink::initialize()
{
    packetsReceived = 0;
    totalBytes = 0;
    EV << "TrafficSink initialized" << endl;
}

void TrafficSink::handleMessage(cMessage *msg)
{
    cPacket *packet = check_and_cast<cPacket*>(msg);
    
    packetsReceived++;
    totalBytes += packet->getByteLength();
    
    EV << "Received packet " << packetsReceived 
       << ", total bytes: " << totalBytes << endl;
    
    delete packet;
}

void TrafficSink::finish()
{
    EV << "TrafficSink received " << packetsReceived 
       << " packets, " << totalBytes << " bytes total" << endl;
    recordScalar("Packets Received", packetsReceived);
    recordScalar("Total Bytes", totalBytes);
    recordScalar("Throughput (bytes/sec)", totalBytes / simTime().dbl());
}