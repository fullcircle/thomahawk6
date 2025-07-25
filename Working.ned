//
// Working Tomahawk 6 Basic Test
//

package tomahawk6;

simple TrafficSource
{
    parameters:
        @display("i=block/source");
        
    gates:
        output out;
}

simple TrafficSink  
{
    parameters:
        @display("i=block/sink");
        
    gates:
        input in;
}

network BasicTomahawk6Test
{
    submodules:
        source: TrafficSource;
        sink: TrafficSink;
        
    connections:
        source.out --> sink.in;
}