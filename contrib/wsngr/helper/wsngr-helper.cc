/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "wsngr-helper.h"

namespace ns3 {

WsngrHelper::WsngrHelper ()
{
  m_agentFactory.SetTypeId ("ns3::wsngr::RoutingProtocol");
}

WsngrHelper::WsngrHelper (const WsngrHelper &o)
  : m_agentFactory (o.m_agentFactory)
{
  m_interfaceExclusions = o.m_interfaceExclusions;
}

WsngrHelper*
WsngrHelper::Copy (void) const
{
  return new WsngrHelper (*this);
}

// void
// WsngrHelper::ExcludeInterface (Ptr<Node> node, uint32_t interface)
// {
//   std::map< Ptr<Node>, std::set<uint32_t> >::iterator it = m_interfaceExclusions.find (node);

//   if (it == m_interfaceExclusions.end ())
//     {
//       std::set<uint32_t> interfaces;
//       interfaces.insert (interface);

//       m_interfaceExclusions.insert (std::make_pair (node, std::set<uint32_t> (interfaces) ));
//     }
//   else
//     {
//       it->second.insert (interface);
//     }
// }

Ptr<Ipv4RoutingProtocol>
WsngrHelper::Create (Ptr<Node> node) const
{
  Ptr<wsngr::RoutingProtocol> agent = m_agentFactory.Create<wsngr::RoutingProtocol> ();
  node->AggregateObject (agent);

  return agent;
}

void WsngrHelper::Install (NodeContainer c) const
{
  // NodeContainer c = NodeContainer::GetGlobal ();
//   for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
//     {
//       Ptr<Node> node = (*i);
//       Ptr<UdpL4Protocol> udp = node->GetObject<UdpL4Protocol> ();
//       Ptr<egsr::RoutingProtocol> egsr = node->GetObject<egsr::RoutingProtocol> ();
//       egsr->SetDownTarget (udp->GetDownTarget ());
//       udp->SetDownTarget (MakeCallback(&egsr::RoutingProtocol::AddHeader, egsr));
//     }
}


void
WsngrHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}

}

