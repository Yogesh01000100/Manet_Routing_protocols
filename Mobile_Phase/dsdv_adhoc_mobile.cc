#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DsdvExample");

int main (int argc, char *argv[])
{
    // Enable DSDV logs
    LogComponentEnable ("DsdvRoutingProtocol", LOG_LEVEL_DEBUG);

    // Create n nodes
    NodeContainer nodes;
    nodes.Create (25);

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


    // Install Internet stack and DSDV
    DsdvHelper dsdv;
    InternetStackHelper stack;
    stack.SetRoutingHelper (dsdv);
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
    for (int i = 1; i < 25; ++i) {
        clientApps.Add (client.Install (nodes.Get (i)));
    }
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (20.0));

    // Enable routing table logging
     Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("dsdv_mobile.routes", std::ios::out);
    dsdv.PrintRoutingTableAllAt (Seconds (5), routingStream);

    Simulator::Stop (Seconds (20.0));

    AnimationInterface anim("dsdv_adhoc_mobile.xml");

    Simulator::Run ();
    Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApps.Get (0));
    uint32_t totalBytesReceived = sink->GetTotalRx ();
    double simulationDuration = 20.0;
    double throughput = (totalBytesReceived * 8.0) / (simulationDuration * 1024); // kbps

    std::cout << "Total Bytes Received: " << totalBytesReceived << " bytes" << std::endl;
    std::cout << "Throughput: " << throughput << " kbps" << std::endl;
    Simulator::Destroy ();
    
    return 0;
}

