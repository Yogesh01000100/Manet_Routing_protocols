#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;
Time g_firstRxTime (Seconds (0));
Time g_lastRxTime (Seconds (0));

// Custom callback function
void ReceivePacket (Ptr<const Packet> packet, const Address &)
{
    Time now = Simulator::Now ();
    if (g_firstRxTime.IsZero ())
    {
        g_firstRxTime = now;
    }
    g_lastRxTime = now;
}
NS_LOG_COMPONENT_DEFINE ("OlsrExample");

int main (int argc, char *argv[])
{
    // Enable OLSR logs
    LogComponentEnable ("OlsrRoutingProtocol", LOG_LEVEL_DEBUG);

    // Create n nodes
    NodeContainer nodes;
    nodes.Create (5);

    // Set up Wi-Fi devices and channel
    WifiHelper wifi;
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", 
                                  "DataMode", StringValue ("OfdmRate6Mbps"));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper();
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

    // Implement Mobility
MobilityHelper mobility;
mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                               "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                               "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));

mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                           "Bounds", RectangleValue (Rectangle (0, 50, 0, 50)),
                           "Distance", DoubleValue (10),
                           "Time", TimeValue (Seconds (1)));
mobility.Install (nodes);


    // Install Internet stack and OLSR
    OlsrHelper olsr;
    InternetStackHelper stack;
    stack.SetRoutingHelper (olsr);
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Create and install applications (e.g., UDP echo client-server)
    uint16_t port = 9;
    Address sinkAddress (InetSocketAddress (interfaces.GetAddress (0), port));
    PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (0));
    sinkApps.Start (Seconds (1.0));
    sinkApps.Stop (Seconds (20.0));

    UdpEchoClientHelper client (interfaces.GetAddress (0), port);
    client.SetAttribute ("MaxPackets", UintegerValue (100));
    client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    client.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps;
    for (int i = 1; i < 5; ++i) {
        clientApps.Add (client.Install (nodes.Get (i)));
    }
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (20.0));

    // Enable routing table logging
     Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("olsr_mobile.routes", std::ios::out);
    olsr.PrintRoutingTableAllAt (Seconds (5), routingStream);

    Simulator::Stop (Seconds (20.0));

    // Set up the animation interface
    AnimationInterface anim("olsr_adhoc_mobile.xml");
    Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApps.Get (0));
    sink->TraceConnectWithoutContext ("Rx", MakeCallback (&ReceivePacket));
    
    Simulator::Run ();
    //Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApps.Get (0));
    uint32_t totalBytesReceived = sink->GetTotalRx ();

    double actualDuration = g_lastRxTime.GetSeconds () - g_firstRxTime.GetSeconds ();
    double throughput = (totalBytesReceived * 8.0) / (actualDuration * 1024); // kbps
    
    std::cout << "Actual Simulation time: " << actualDuration << " secs " << std::endl;
    std::cout << "Total Bytes Received: " << totalBytesReceived << " bytes" << std::endl;
    std::cout << "Throughput: " << throughput << " kbps" << std::endl;
    Simulator::Destroy ();
    
    return 0;
}
