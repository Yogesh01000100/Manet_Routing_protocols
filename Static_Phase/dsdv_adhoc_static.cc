#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/dsdv-module.h"
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

NS_LOG_COMPONENT_DEFINE ("DsdvExample");
uint32_t totalBytesReceived = 0;

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


    // Set static mobility for each node
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	// Define node positions here
	for (uint32_t i = 0; i < nodes.GetN(); ++i)
	{
	    positionAlloc->Add (Vector (5.0 * i, 5.0 * i, 0.0)); // in a diagonal line
	}
    mobility.SetPositionAllocator (positionAlloc);
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

    // Setup UdpEchoClient applications
    UdpEchoClientHelper client (interfaces.GetAddress (0), port);
    client.SetAttribute ("MaxPackets", UintegerValue (100));
    client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    client.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        clientApps.Add (client.Install (nodes.Get (i)));
    }
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (20.0));

    // Enable routing table logging
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("dsdv_static.routes", std::ios::out);
    dsdv.PrintRoutingTableAllAt (Seconds (5), routingStream);

    Simulator::Stop (Seconds (20.0));

    AnimationInterface anim("dsdv_adhoc_static.xml");
    
    Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApps.Get (0));
    sink->TraceConnectWithoutContext ("Rx", MakeCallback (&ReceivePacket));

    Simulator::Run ();

    uint32_t totalBytesReceived = sink->GetTotalRx ();
    double actualDuration = g_lastRxTime.GetSeconds () - g_firstRxTime.GetSeconds ();
    double throughput = (totalBytesReceived * 8.0) / (actualDuration * 1024); // kbps

    std::cout << "Actual Simulation time: " << actualDuration << " secs " << std::endl;
    std::cout << "Total Bytes Received: " << totalBytesReceived << " bytes" << std::endl;
    std::cout << "Throughput: " << throughput << " kbps" << std::endl;

    Simulator::Destroy ();
    
    return 0;
}
