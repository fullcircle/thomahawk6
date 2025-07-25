#ifndef __TOMAHAWK6_SERDESCORE_H_
#define __TOMAHAWK6_SERDESCORE_H_

#include <omnetpp.h>
#include "inet/common/INETDefs.h"
#include "inet/common/packet/Packet.h"

using namespace omnetpp;
using namespace inet;

namespace tomahawk6 {

/**
 * SerDes Core implementation for Tomahawk 6
 * Supports both 106.25G PAM4 and 212.5G PAM4 configurations
 */
class INET_API SerDesCore : public cSimpleModule
{
  private:
    // Configuration parameters
    std::string serdesType;
    double dataRate;
    simtime_t latency;
    
    // Statistics
    simsignal_t throughputSignal;
    simsignal_t utilizationSignal;
    
    // State tracking
    bool busy;
    simtime_t transmissionStartTime;
    cMessage *endTransmissionTimer;
    
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    
    // SerDes specific functions
    virtual simtime_t calculateTransmissionTime(cPacket *packet);
    virtual void startTransmission(cPacket *packet);
    virtual void endTransmission();
    
  public:
    SerDesCore();
    virtual ~SerDesCore();
    
    // Public interface for capacity queries
    double getDataRate() const { return dataRate; }
    std::string getSerDesType() const { return serdesType; }
    bool isBusy() const { return busy; }
};

} // namespace tomahawk6

#endif