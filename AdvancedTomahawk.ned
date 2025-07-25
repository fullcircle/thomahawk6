//
// Advanced Tomahawk 6 Switch with AI Traffic Generation
//

package tomahawk6;

simple AdvancedTrafficGen
{
    parameters:
        @display("i=block/source");
        string workloadType = default("AllReduce");
        double trafficIntensity = default(0.8);
        int tensorSize @unit(B) = default(100MB);
        int numGPUs = default(8);
        bool rocevProtocol = default(true);
        
    gates:
        output out;
}

simple AdvancedSink
{
    parameters:
        @display("i=block/sink");
        
    gates:
        input in;
}

simple SimpleSwitch
{
    parameters:
        @display("i=device/switch");
        string portConfig = default("8x100G");
        double capacity @unit(bps) = default(800Gbps);
        
    gates:
        input in[];
        output out[];
}

network AdvancedTomahawk6Network
{
    parameters:
        int numSources = default(4);
        
    submodules:
        trafficGen[numSources]: AdvancedTrafficGen {
            parameters:
                workloadType = index % 2 == 0 ? "AllReduce" : "AllGather";
                tensorSize = 1GB;
                numGPUs = 64;
        }
        
        switch: SimpleSwitch {
            gates:
                in[parent.numSources];
                out[parent.numSources];
        }
        
        sink[numSources]: AdvancedSink;
        
    connections:
        for i=0..numSources-1 {
            trafficGen[i].out --> switch.in[i];
            switch.out[i] --> sink[i].in;
        }
}