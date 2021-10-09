/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef WSNGR_H
#define WSNGR_H

#include "ns3/test.h"
#include "wsngr-state.h"

#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/event-garbage-collector.h"
#include "ns3/random-variable-stream.h"
#include "ns3/timer.h"
#include "ns3/traced-callback.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-static-routing.h"
#include"ns3/vector.h"
#include <vector>
#include <map>
#include<iostream>
#include<unordered_map>

namespace ns3 {
namespace wsngr{
constexpr double ALPHA = 0.01;

enum NodeState{
  CHARGING,WORKING,DEAD
};

struct NodeInfo{

  constexpr static double MAX_ENERGY = 10.8;
  constexpr static double CHARING_THRESHOLD = 1;
  constexpr static int RX_TYPE = 0; 
  constexpr static int TX_TYPE = 1; 
  constexpr static int SENSING_TYPE = 2; 

  constexpr static double ENERGY_RECORD_INTERVEL = 60 * 10;

  Ipv4Address ip;
  Vector position;
  double energy = MAX_ENERGY;
  double energy_consume_in_record_intervel = 0;
  Time last_update_time;
  NodeState state;

  Time requested_time;

  int deadTimes = 0;

  void handleEnergy(int type);

  double getEnergyRate()const;
};

std::ostream& operator<<(std::ostream& os,const NodeInfo& node);

class RoutingProtocol : public Ipv4RoutingProtocol
{
public:
  struct IpHash{
    size_t operator()(const Ipv4Address& ip)const{
        return std::hash<uint32_t>{}(ip.Get());
    }
  };
  static std::unordered_map<Ipv4Address,NodeInfo,IpHash> nodes;

  static Ipv4Address sink_ip;

  static void SetSinkIP(Ipv4Address ip){
    sink_ip = ip;
  }

  static NodeInfo& getSink(){
    return nodes[sink_ip];
  }

  static std::unordered_map<Ipv4Address,NodeInfo,IpHash>& GetNodes(){
    return nodes;
  }

  static constexpr double Tx_Consume = 0.01 * 1.5;
  static constexpr double Rx_Consume = Tx_Consume * 0.6;
  static constexpr double Sensing_Consume = 0.01 * 1.4;

  /**
   * \brief Get the type ID.
   * \return The object TypeId.
   */
  static TypeId GetTypeId (void);

  RoutingProtocol (void);
  virtual ~RoutingProtocol (void);

  /**
   * \brief Set the OLSR main address to the first address on the indicated interface.
   *
   * \param interface IPv4 interface index
   */
  void SetMainInterface (uint32_t interface);

private:
  std::set<uint32_t> m_interfaceExclusions; //!< Set of interfaces excluded by OSLR.
  Ptr<Ipv4StaticRouting> m_routingTableAssociation; //!< Associations from an Ipv4StaticRouting instance

public:
  /**
   * Get the excluded interfaces.
   * \returns Container of excluded interfaces.
   */
  std::set<uint32_t> GetInterfaceExclusions (void) const
  {
    return m_interfaceExclusions;
  }
protected:
  virtual void DoInitialize (void);
  virtual void DoDispose (void);

private:

  WsngrState m_state;  //!< Internal state with all needed data structs.
  Ptr<Ipv4> m_ipv4;   //!< IPv4 object the routing is linked to.

public:
  // From Ipv4RoutingProtocol
  virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p,
                                      const Ipv4Header &header,
                                      Ptr<NetDevice> oif,
                                      Socket::SocketErrno &sockerr);
  virtual bool RouteInput (Ptr<const Packet> p,
                           const Ipv4Header &header,
                           Ptr<const NetDevice> idev,
                           UnicastForwardCallback ucb,
                           MulticastForwardCallback mcb,
                           LocalDeliverCallback lcb,
                           ErrorCallback ecb);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);

  /**
   * \returns the ipv4 object this routing protocol is associated with
   */
  NS_DEPRECATED_3_34
  virtual Ptr<Ipv4> GetIpv4 (void) const;
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;


private:
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);

public:
  /**
   * \brief Gets the main address associated with a given interface address.
   * \param iface_addr the interface address.
   * \return the corresponding main address.
   */
  Ipv4Address GetMainAddress (Ipv4Address iface_addr) const;

  Ipv4Address GetNextForward();

private:
  Ipv4Address m_mainAddress; //!< the node main address.

  /// Provides uniform random variables.
  Ptr<UniformRandomVariable> m_uniformRandomVariable;

  NodeInfo& GetInfo();

  Timer m_sensingTimer;

  void SensingConsume();

};

}
}

#endif /* WSNGR_H */

