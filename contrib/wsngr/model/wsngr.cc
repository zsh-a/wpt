#define NS_LOG_APPEND_CONTEXT                                        \
  if (GetObject<Node> ())                                            \
    {                                                                \
      std::clog << "[node " << GetObject<Node> ()->GetId () << "] "; \
    }

#include <iomanip>
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-route.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/enum.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-packet-info-tag.h"
#include "wsngr.h"

#include <algorithm>
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wwrite-strings"
#pragma GCC diagnostic ignored "-Wparentheses"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("WsngrRoutingProtocol");

namespace wsngr {

constexpr double COMM_RANGE = 30;

const auto SENSING_ENERGY_UPDATE_INTERVAL = Seconds(10);

void NodeInfo::handleEnergy(int type){

  if(state == NodeState::CHARGING || state == NodeState::DEAD){
    return;
  }
  double consume = 0;
  auto now = Simulator::Now();
  if((now - last_update_time).GetSeconds() >= ENERGY_RECORD_INTERVEL){
    last_update_time = now;
    energy_consume_in_record_intervel = 0;
  }
  last_update_time = Simulator::Now();

  if(type == TX_TYPE){ // Tx
    consume = RoutingProtocol::Tx_Consume;
  }else if(type == RX_TYPE){ //Rx
    consume = RoutingProtocol::Rx_Consume;
  }else if(type == SENSING_TYPE){
    consume = RoutingProtocol::Sensing_Consume;
  }
  energy_consume_in_record_intervel += consume;
  energy -= consume;
  if(energy <= MIN_ENERGY){
    if(state != NodeState::DEAD){
      deadTimes++;
      state = NodeState::DEAD;
      energy = MIN_ENERGY;
    }
  }
}

double NodeInfo::getEnergyRate()const{
  return energy_consume_in_record_intervel / ENERGY_RECORD_INTERVEL;
}

std::ostream& operator<<(std::ostream& os,const NodeInfo& node){

  os << "ip : " << node.ip << " energy : " << node.energy << " rate : " << node.getEnergyRate() << " dead times : " << node.deadTimes;
}

std::unordered_map<Ipv4Address, NodeInfo, RoutingProtocol::IpHash> RoutingProtocol::nodes{};
Ipv4Address RoutingProtocol::sink_ip;

/********** OLSR class **********/

NS_OBJECT_ENSURE_REGISTERED (RoutingProtocol);

TypeId
RoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::wsngr::RoutingProtocol")
                          .SetParent<Ipv4RoutingProtocol> ()
                          .SetGroupName ("Wsngr")
                          .AddConstructor<RoutingProtocol> ();
  return tid;
}

RoutingProtocol::RoutingProtocol (void) : m_ipv4 (0),
  m_sensingTimer(Timer::CANCEL_ON_DESTROY)
{
  m_uniformRandomVariable = CreateObject<UniformRandomVariable> ();
}

RoutingProtocol::~RoutingProtocol (void)
{
}

void
RoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_ASSERT (ipv4 != 0);
  NS_ASSERT (m_ipv4 == 0);
  NS_LOG_DEBUG ("Created olsr::RoutingProtocol");

  m_ipv4 = ipv4;
  m_sensingTimer.SetFunction(&RoutingProtocol::SensingConsume, this);
}

Ptr<Ipv4>
RoutingProtocol::GetIpv4 (void) const
{
  return m_ipv4;
}

void
RoutingProtocol::DoDispose (void)
{
  m_ipv4 = 0;
  Ipv4RoutingProtocol::DoDispose ();
}

void
RoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  // std::ostream* os = stream->GetStream ();
  // // Copy the current ostream state
  // std::ios oldState (nullptr);
  // oldState.copyfmt (*os);

  // *os << std::resetiosflags (std::ios::adjustfield) << std::setiosflags (std::ios::left);

  // *os << "Node: " << m_ipv4->GetObject<Node> ()->GetId ()
  //     << ", Time: " << Now ().As (unit)
  //     << ", Local time: " << m_ipv4->GetObject<Node> ()->GetLocalTime ().As (unit)
  //     << ", OLSR Routing table" << std::endl;

  // *os << std::setw (16) << "Destination";
  // *os << std::setw (16) << "NextHop";
  // *os << std::setw (16) << "Interface";
  // *os << "Distance" << std::endl;

  // for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator iter = m_table.begin ();
  //      iter != m_table.end (); iter++)
  //   {
  //     *os << std::setw (16) << iter->first;
  //     *os << std::setw (16) << iter->second.nextAddr;
  //     *os << std::setw (16);
  //     if (Names::FindName (m_ipv4->GetNetDevice (iter->second.interface)) != "")
  //       {
  //         *os << Names::FindName (m_ipv4->GetNetDevice (iter->second.interface));
  //       }
  //     else
  //       {
  //         *os << iter->second.interface;
  //       }
  //     *os << iter->second.distance << std::endl;
  //   }
  // *os << std::endl;

  // // Also print the HNA routing table
  // if (m_hnaRoutingTable->GetNRoutes () > 0)
  //   {
  //     *os << "HNA Routing Table:" << std::endl;
  //     m_hnaRoutingTable->PrintRoutingTable (stream, unit);
  //   }
  // else
  //   {
  //     *os << "HNA Routing Table: empty" << std::endl;
  //   }
  // // Restore the previous ostream state
  // (*os).copyfmt (oldState);
}

void
RoutingProtocol::DoInitialize ()
{
  if (m_mainAddress == Ipv4Address ())
    {
      Ipv4Address loopback ("127.0.0.1");
      for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
        {
          // Use primary address, if multiple
          Ipv4Address addr = m_ipv4->GetAddress (i, 0).GetLocal ();
          if (addr != loopback)
            {
              m_mainAddress = addr;
              break;
            }
        }

      NS_ASSERT (m_mainAddress != Ipv4Address ());
    }

  
    for (auto &[ip, info] : RoutingProtocol::nodes)
    {

      // std::cout << "ip : " << ip << " " << info.position << "\n";
      if (ip == m_mainAddress)
        continue;
      double dist =
          CalculateDistance (info.position, RoutingProtocol::nodes[m_mainAddress].position);
      if (dist <= COMM_RANGE)
        {
          m_state.GetNeighbors ().push_back (ip);
        }
    }
  // std::cout << RoutingProtocol::nodes.size()<<"~~~~~~~~~~~~\n";

  
  auto& sink = RoutingProtocol::nodes[RoutingProtocol::sink_ip];
  auto& ne_vec = m_state.GetNeighbors ();
  std::sort (ne_vec.begin (), ne_vec.end (), [&] (const auto &a, const auto &b) {
    auto x = RoutingProtocol::nodes[a], y = RoutingProtocol::nodes[b];
    return CalculateDistance (x.position, sink.position) <
           CalculateDistance (y.position, sink.position);
  });
  NS_LOG_DEBUG ("Starting OLSR on node " << m_mainAddress);

  Ipv4Address loopback ("127.0.0.1");

  // bool canRunOlsr = false;
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
    {
      Ipv4Address addr = m_ipv4->GetAddress (i, 0).GetLocal ();
      if (addr == loopback)
        {
          continue;
        }

      if (addr != m_mainAddress)
        {
          // Create never expiring interface association tuple entries for our
          // own network interfaces, so that GetMainAddress () works to
          // translate the node's own interface addresses into the main address.
          // IfaceAssocTuple tuple;
          // tuple.ifaceAddr = addr;
          // tuple.mainAddr = m_mainAddress;
          // AddIfaceAssocTuple (tuple);
          NS_ASSERT (GetMainAddress (addr) == m_mainAddress);
        }

      if (m_interfaceExclusions.find (i) != m_interfaceExclusions.end ())
        {
          continue;
        }

      // // Create a socket to listen on all the interfaces
      // if (m_recvSocket == 0)
      //   {
      //     m_recvSocket = Socket::CreateSocket (GetObject<Node> (),
      //                                          UdpSocketFactory::GetTypeId ());
      //     m_recvSocket->SetAllowBroadcast (true);
      //     InetSocketAddress inetAddr (Ipv4Address::GetAny (), OLSR_PORT_NUMBER);
      //     m_recvSocket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvOlsr,  this));
      //     if (m_recvSocket->Bind (inetAddr))
      //       {
      //         NS_FATAL_ERROR ("Failed to bind() OLSR socket");
      //       }
      //     m_recvSocket->SetRecvPktInfo (true);
      //     m_recvSocket->ShutdownSend ();
      //   }

      // Create a socket to send packets from this specific interfaces
      // Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
      //                                            UdpSocketFactory::GetTypeId ());
      // socket->SetAllowBroadcast (true);
      // socket->SetIpTtl (1);
      // InetSocketAddress inetAddr (m_ipv4->GetAddress (i, 0).GetLocal (), OLSR_PORT_NUMBER);
      // socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvOlsr,  this));
      // socket->BindToNetDevice (m_ipv4->GetNetDevice (i));
      // if (socket->Bind (inetAddr))
      //   {
      //     NS_FATAL_ERROR ("Failed to bind() OLSR socket");
      //   }
      // socket->SetRecvPktInfo (true);
      // m_sendSockets[socket] = m_ipv4->GetAddress (i, 0);

      // canRunOlsr = true;
    }

  // if (canRunOlsr)
  //   {
  //     HelloTimerExpire ();
  //     TcTimerExpire ();
  //     MidTimerExpire ();
  //     HnaTimerExpire ();

  //     NS_LOG_DEBUG ("OLSR on node " << m_mainAddress << " started");
  //   }
  if(m_mainAddress != sink_ip)
    SensingConsume();
}

void
RoutingProtocol::SetMainInterface (uint32_t interface)
{
  m_mainAddress = m_ipv4->GetAddress (interface, 0).GetLocal ();
}

Ipv4Address
RoutingProtocol::GetMainAddress (Ipv4Address iface_addr) const
{
  // const IfaceAssocTuple *tuple = m_state.FindIfaceAssocTuple (iface_addr);

  // if (tuple != NULL)
  //   {
  //     return tuple->mainAddr;
  //   }
  // else
  //   {
  //     return iface_addr;
  //   }
  return m_mainAddress;
}


double RoutingProtocol::get_weigth(Ipv4Address ip,double weight){
  auto& node = RoutingProtocol::nodes[ip];
  if(node.energy <= NodeInfo::MIN_ENERGY) return 1e9;
  auto& sink = RoutingProtocol::getSink();
  auto dist = CalculateDistanceSquared(node.position,sink.position);
  return atan(dist) * weight + (1 - weight) / atan(node.energy);
};

Ipv4Address
RoutingProtocol::GetNextForward ()
{
  if (!RoutingProtocol::nodes.count (sink_ip))
    return Ipv4Address::GetLoopback ();

  auto& sink = RoutingProtocol::nodes[RoutingProtocol::sink_ip];
  auto& ne_vec = m_state.GetNeighbors ();

  if (auto it = std::find (ne_vec.begin (), ne_vec.end (), sink_ip); it != ne_vec.end ())
  {
    return sink_ip;
  }

  Ipv4Address ret = Ipv4Address::GetLoopback();

  std::sort(ne_vec.begin(),ne_vec.end(),[this](const auto& a,const auto& b){
    return get_weigth(a,1) < get_weigth(b,1);
  });

  for(auto& ip : ne_vec){
    auto& node = RoutingProtocol::nodes[ip];
    if(node.energy > NodeInfo::MIN_ENERGY) return ip;
  }
  return Ipv4Address::GetLoopback ();
}

Ptr<Ipv4Route>
RoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif,
                              Socket::SocketErrno &sockerr)
{
  // NS_LOG_FUNCTION (this << " " << m_ipv4->GetObject<Node> ()->GetId () << " " << header.GetDestination () << " " << oif);
  Ptr<Ipv4Route> rtentry;
  // RoutingTableEntry entry1, entry2;
  // bool found = false;
  Ipv4Address next = GetNextForward ();
  // std::cout << "next " << next << '\n';

  auto sink = header.GetDestination();

  if (next.IsLocalhost () || next == header.GetSource())
    {
      rtentry = Create<Ipv4Route> ();
      rtentry->SetDestination (header.GetDestination ());
      rtentry->SetSource (m_ipv4->GetAddress (1, 0).GetLocal ());
      rtentry->SetGateway (Ipv4Address::GetLoopback ());
      rtentry->SetOutputDevice (m_ipv4->GetNetDevice (0));
      sockerr = Socket::ERROR_NOTERROR;
    }
  else
    {
      rtentry = Create<Ipv4Route> ();
      rtentry->SetDestination (header.GetDestination ());

      rtentry->SetSource (m_mainAddress);
      rtentry->SetGateway (next);
      for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
        {
          for (uint32_t j = 0; j < m_ipv4->GetNAddresses (i); j++)
            {
              if (m_ipv4->GetAddress (i, j).GetLocal () == m_mainAddress)
                {
                  rtentry->SetOutputDevice (m_ipv4->GetNetDevice (i));
                  break;
                }
            }
        }

      // std::cout << "path " << m_mainAddress << "->" << next << '\n';
      sockerr = Socket::ERROR_NOTERROR;
    }
  GetInfo().handleEnergy(NodeInfo::TX_TYPE);

  return rtentry;
  // if (Lookup (header.GetDestination (), entry1) != 0)
  //   {
  //     bool foundSendEntry = FindSendEntry (entry1, entry2);
  //     if (!foundSendEntry)
  //       {
  //         NS_FATAL_ERROR ("FindSendEntry failure");
  //       }
  //     uint32_t interfaceIdx = entry2.interface;
  //     if (oif && m_ipv4->GetInterfaceForDevice (oif) != static_cast<int> (interfaceIdx))
  //       {
  //         // We do not attempt to perform a constrained routing search
  //         // if the caller specifies the oif; we just enforce that
  //         // that the found route matches the requested outbound interface
  //         NS_LOG_DEBUG ("Olsr node " << m_mainAddress
  //                                    << ": RouteOutput for dest=" << header.GetDestination ()
  //                                    << " Route interface " << interfaceIdx
  //                                    << " does not match requested output interface "
  //                                    << m_ipv4->GetInterfaceForDevice (oif));
  //         sockerr = Socket::ERROR_NOROUTETOHOST;
  //         return rtentry;
  //       }
  //     rtentry = Create<Ipv4Route> ();
  //     rtentry->SetDestination (header.GetDestination ());
  //     // the source address is the interface address that matches
  //     // the destination address (when multiple are present on the
  //     // outgoing interface, one is selected via scoping rules)
  //     NS_ASSERT (m_ipv4);
  //     uint32_t numOifAddresses = m_ipv4->GetNAddresses (interfaceIdx);
  //     NS_ASSERT (numOifAddresses > 0);
  //     Ipv4InterfaceAddress ifAddr;
  //     if (numOifAddresses == 1)
  //       {
  //         ifAddr = m_ipv4->GetAddress (interfaceIdx, 0);
  //       }
  //     else
  //       {
  //         /// \todo Implement IP aliasing and OLSR
  //         NS_FATAL_ERROR ("XXX Not implemented yet:  IP aliasing and OLSR");
  //       }
  //     rtentry->SetSource (ifAddr.GetLocal ());
  //     rtentry->SetGateway (entry2.nextAddr);
  //     rtentry->SetOutputDevice (m_ipv4->GetNetDevice (interfaceIdx));
  //     sockerr = Socket::ERROR_NOTERROR;
  //     NS_LOG_DEBUG ("Olsr node " << m_mainAddress
  //                                << ": RouteOutput for dest=" << header.GetDestination ()
  //                                << " --> nextHop=" << entry2.nextAddr
  //                                << " interface=" << entry2.interface);
  //     NS_LOG_DEBUG ("Found route to " << rtentry->GetDestination () << " via nh " << rtentry->GetGateway () << " with source addr " << rtentry->GetSource () << " and output dev " << rtentry->GetOutputDevice ());
  //     found = true;
  //   }
  // else
  //   {
  //     rtentry = m_hnaRoutingTable->RouteOutput (p, header, oif, sockerr);

  //     if (rtentry)
  //       {
  //         found = true;
  //         NS_LOG_DEBUG ("Found route to " << rtentry->GetDestination () << " via nh " << rtentry->GetGateway () << " with source addr " << rtentry->GetSource () << " and output dev " << rtentry->GetOutputDevice ());
  //       }
  //   }

  // if (!found)
  //   {
  //     NS_LOG_DEBUG ("Olsr node " << m_mainAddress
  //                                << ": RouteOutput for dest=" << header.GetDestination ()
  //                                << " No route to host");
  //     sockerr = Socket::ERROR_NOROUTETOHOST;
  //   }
}

bool
RoutingProtocol::RouteInput (Ptr<const Packet> p, const Ipv4Header &header,
                             Ptr<const NetDevice> idev, UnicastForwardCallback ucb,
                             MulticastForwardCallback mcb, LocalDeliverCallback lcb,
                             ErrorCallback ecb)
{
  //   NS_LOG_FUNCTION (this << " " << m_ipv4->GetObject<Node> ()->GetId () << " " << header.GetDestination ());
  GetInfo().handleEnergy(NodeInfo::RX_TYPE);

  Ipv4Address dst = header.GetDestination ();
  Ipv4Address origin = header.GetSource ();

  //   // Consume self-originated packets
  if (m_mainAddress == dst)
    {
      return true;
    }

  // Local delivery
  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
  uint32_t iif = m_ipv4->GetInterfaceForDevice (idev);
  if (m_ipv4->IsDestinationAddress (dst, iif))
    {
      if (!lcb.IsNull ())
        {
          NS_LOG_LOGIC ("Local delivery to " << dst);
          lcb (p, header, iif);
          return true;
        }
      else
        {
          // The local delivery callback is null.  This may be a multicast
          // or broadcast packet, so return false so that another
          // multicast routing protocol can handle it.  It should be possible
          // to extend this to explicitly check whether it is a unicast
          // packet, and invoke the error callback if so
          NS_LOG_LOGIC ("Null local delivery callback");
          return false;
        }
    }

  //   NS_LOG_LOGIC ("Forward packet");
  //   // Forwarding
  Ptr<Ipv4Route> rtentry;

  Ipv4Address next = GetNextForward ();

  if (next.IsLocalhost () || next == origin)
    return false;

  rtentry = Create<Ipv4Route> ();
  rtentry->SetDestination (header.GetDestination ());

  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces (); i++)
    {
      for (uint32_t j = 0; j < m_ipv4->GetNAddresses (i); j++)
        {
          if (m_ipv4->GetAddress (i, j).GetLocal () == m_mainAddress)
            {
              rtentry->SetOutputDevice (m_ipv4->GetNetDevice (i));
              break;
            }
        }
    }
  // std::cout << "path : " << m_mainAddress << "->" << next << '\n';
  rtentry->SetSource (m_mainAddress);
  rtentry->SetGateway (next);

  ucb (rtentry, p, header);

  GetInfo().handleEnergy(NodeInfo::TX_TYPE);

  return true;

  // NS_LOG_DEBUG ("Olsr node " << m_mainAddress
  //                            << ": RouteInput for dest=" << header.GetDestination ()
  //                            << " --> nextHop=" << entry2.nextAddr
  //                            << " interface=" << entry2.interface);
}
NodeInfo& RoutingProtocol::GetInfo(){
  return RoutingProtocol::nodes[m_mainAddress];
}



void RoutingProtocol::SensingConsume(){
  GetInfo().handleEnergy(NodeInfo::SENSING_TYPE);
  m_sensingTimer.Schedule(SENSING_ENERGY_UPDATE_INTERVAL);
}

void
RoutingProtocol::NotifyInterfaceUp (uint32_t i)
{
}
void
RoutingProtocol::NotifyInterfaceDown (uint32_t i)
{
}
void
RoutingProtocol::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
}
void
RoutingProtocol::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
}

} // namespace wsngr
} // namespace ns3