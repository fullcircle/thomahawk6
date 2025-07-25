//
// Simple Test Network for Tomahawk 6 Validation
//

package tomahawk6;

simple TestTrafficGen
{
    parameters:
        @display("i=block/source");
        double interval @unit(s) = default(1s);
        
    gates:
        output out;
}

simple TestSink
{
    parameters:
        @display("i=block/sink");
        
    gates:
        input in;
}

network SimpleTestNetwork
{
    submodules:
        gen: TestTrafficGen;
        sink: TestSink;
        
    connections:
        gen.out --> sink.in;
}