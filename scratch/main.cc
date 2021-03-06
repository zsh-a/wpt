/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

//
// This program configures a grid (default 5x5) of nodes on an
// 802.11b physical layer, with
// 802.11b NICs in adhoc mode, and by default, sends one packet of 1000
// (application) bytes to node 1.
//
// The default layout is like this, on a 2-D grid.
//
// n20  n21  n22  n23  n24
// n15  n16  n17  n18  n19
// n10  n11  n12  n13  n14
// n5   n6   n7   n8   n9
// n0   n1   n2   n3   n4
//
// the layout is affected by the parameters given to GridPositionAllocator;
// by default, GridWidth is 5 and numNodes is 25..
//
// There are a number of command-line options available to control
// the default behavior.  The list of available command-line options
// can be listed with the following command:
// ./waf --run "wifi-simple-adhoc-grid --help"
//
// Note that all ns-3 attributes (not just the ones exposed in the below
// script) can be changed at command line; see the ns-3 documentation.
//
// For instance, for this configuration, the physical layer will
// stop successfully receiving packets when distance increases beyond
// the default of 500m.
// To see this effect, try running:
//
// ./waf --run "wifi-simple-adhoc-grid --distance=500"
// ./waf --run "wifi-simple-adhoc-grid --distance=1000"
// ./waf --run "wifi-simple-adhoc-grid --distance=1500"
//
// The source node and sink node can be changed like this:
//
// ./waf --run "wifi-simple-adhoc-grid --sourceNode=20 --sinkNode=10"
//
// This script can also be helpful to put the Wifi layer into verbose
// logging mode; this command will turn on all wifi logging:
//
// ./waf --run "wifi-simple-adhoc-grid --verbose=1"
//
// By default, trace file writing is off-- to enable it, try:
// ./waf --run "wifi-simple-adhoc-grid --tracing=1"
//
// When you are done tracing, you will notice many pcap trace files
// in your directory.  If you have tcpdump installed, you can try this:
//
// tcpdump -r wifi-simple-adhoc-grid-0-0.pcap -nn -tt
//

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/wsngr-helper.h"
#include "ns3/charger.h"
#include"ns3/wsngr.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimpleAdhocGrid");
int reveived_packets = 0;

void ReceivePacket (Ptr<Socket> socket)
{
  while (socket->Recv ())
    {
      // NS_LOG_UNCOND ("Received one packet!");
      reveived_packets++;
    }
}

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize,
                             uint32_t pktCount, Time pktInterval )
{
  if (pktCount > 0)
    {
      socket->Send (Create<Packet> (pktSize));
      Simulator::Schedule (pktInterval, &GenerateTraffic,
                           socket, pktSize,pktCount - 1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}


void print_nodes(){
  for(auto& [ip,info] : wsngr::RoutingProtocol::nodes){
    std::cout << info << std::endl;
  }
  Simulator::Schedule(Seconds(1800),print_nodes);
}

int MaxPacketsNumber = 200*3600;

int SendCount = 0;

void SendRandomPacketToLC_DIS(InetSocketAddress& sinkAddress,NodeContainer& nodes)
{
	int m_packetSize = 1024;
	int sinkPort = 8080;
	int64_t nano = 0;
	int64_t onano = 0;
  
  SendCount++;

  int src = (rand() % (nodes.GetN() - 1)) + 1;


  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");

  Ptr<Socket> source = Socket::CreateSocket (nodes.Get (src), tid);
	source->Bind ();
	source->Connect (sinkAddress);

	uint8_t * buffer = new uint8_t [m_packetSize];
	onano = nano = Simulator::Now().GetNanoSeconds();
	for(int i = 0; i<8; i++)
	{
		int64_t tmp = nano >> 8;
		int64_t tmp1 = tmp << 8;
		uint8_t val = (uint8_t)(nano - tmp1);
		nano = tmp;
		buffer[i] = val;
	}

	Ptr<Packet> packet = Create<Packet> (buffer, m_packetSize);
  source->SetIpTtl(10);
	source->Send (packet);
	source->Close();

	if(SendCount < MaxPacketsNumber)
		Simulator::Schedule(Seconds(5), &SendRandomPacketToLC_DIS,sinkAddress,nodes);
}


int main (int argc, char *argv[])
{
  std::string phyMode ("DsssRate1Mbps");
  double distance = 500;  // m
  uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 1;
  uint32_t numNodes = 100;  // by default, 5x5
  uint32_t sinkNode = 0;
  uint32_t sourceNode = 4;
  double interval = 1.0; // seconds
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("tracing", "turn on ascii and pcap tracing", tracing);
  cmd.AddValue ("numNodes", "number of nodes", numNodes);
  cmd.AddValue ("sinkNode", "Receiver node number", sinkNode);
  cmd.AddValue ("sourceNode", "Sender node number", sourceNode);
  cmd.Parse (argc, argv);
  // Convert to time object
  Time interPacketInterval = Seconds (interval);

  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                      StringValue (phyMode));

  NodeContainer c;
  c.Create (numNodes);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;
  if (verbose)
    {
      wifi.EnableLogComponents ();  // Turn on all Wifi logging
    }

  YansWifiPhyHelper wifiPhy;
  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (-10) );
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add an upper mac and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);

  MobilityHelper mobility;
  
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=-100.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=-100.0|Max=100.0]"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);

  // Enable OLSR
  WsngrHelper wsngr;

  Ipv4ListRoutingHelper list;
  list.Add (wsngr, 10);

  InternetStackHelper internet;
  internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (c);

  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  std::cout << "sink addr : " << i.GetAddress (sinkNode, 0) << std::endl;

  c.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector{0,0,0});

  wsngr::RoutingProtocol::SetSinkIP(i.GetAddress (sinkNode, 0));

  for(int j = 0;j < c.GetN();j++){
    auto ip = i.GetAddress (j, 0);
    auto p = c.Get(j);
    std::cout << j + 1 << ":"<< p->GetObject<MobilityModel>()->GetPosition() <<  std::endl;

    wsngr::RoutingProtocol::nodes[ip] = wsngr::NodeInfo{ip,p->GetObject<MobilityModel>()->GetPosition(),wsngr::NodeInfo::MAX_ENERGY,wsngr::NodeInfo::MAX_ENERGY,Simulator::Now(),wsngr::NodeState::WORKING};
  }

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (sinkNode), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> source = Socket::CreateSocket (c.Get (sourceNode), tid);
  InetSocketAddress remote = InetSocketAddress (i.GetAddress (sinkNode, 0), 80);
  source->Connect (remote);
  // if (tracing == true)
  //   {
  //     AsciiTraceHelper ascii;
  //     wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("wifi-simple-adhoc-grid.tr"));
  //     wifiPhy.EnablePcap ("wifi-simple-adhoc-grid", devices);
  //     // Trace routing tables
  //     Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("wifi-simple-adhoc-grid.routes", std::ios::out);
  //     olsr.PrintRoutingTableAllEvery (Seconds (2), routingStream);
  //     Ptr<OutputStreamWrapper> neighborStream = Create<OutputStreamWrapper> ("wifi-simple-adhoc-grid.neighbors", std::ios::out);
  //     olsr.PrintNeighborCacheAllEvery (Seconds (2), neighborStream);

  //     // To do-- enable an IP-level trace that shows forwarding events only
  //   }

  // Give OLSR time to converge-- 30 seconds perhaps
  // Simulator::Schedule (Seconds (30.0), &GenerateTraffic,
  //                      source, packetSize, numPackets, interPacketInterval);

  auto charger = Singleton<charger::ChargerBase>::Get();
  Simulator::Schedule(Seconds (10),&charger::ChargerBase::run,charger);
  
  Simulator::Schedule (Seconds (30.0), &SendRandomPacketToLC_DIS,remote,c);
  Simulator::Schedule(Seconds(1800),print_nodes);
  
  // Output what we are doing
  // NS_LOG_UNCOND ("Testing from node " << sourceNode << " to " << sinkNode << " with grid distance " << distance);
  // Simulator::Schedule (Seconds (200*3600 - 100), &print_nodes);

  Simulator::Stop (Seconds (3*3600));
  Simulator::Run ();
  Simulator::Destroy ();
  print_nodes();
  std::cout << "send packets : " << SendCount  << "\n" << "received packets : " << reveived_packets << "\n";
  charger->print_statistics();
  return 0;
}

