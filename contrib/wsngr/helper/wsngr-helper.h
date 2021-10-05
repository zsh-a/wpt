/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef WSNGR_HELPER_H
#define WSNGR_HELPER_H
#include "ns3/object-factory.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/wsngr.h"

namespace ns3 {

/**
 * \ingroup olsr
 *
 * \brief Helper class that adds OLSR routing to nodes.
 *
 * This class is expected to be used in conjunction with
 * ns3::InternetStackHelper::SetRoutingHelper
 */
class WsngrHelper : public Ipv4RoutingHelper
{
public:
  /**
   * Create an OlsrHelper that makes life easier for people who want to install
   * OLSR routing to nodes.
   */
  WsngrHelper ();

  /**
   * \brief Construct an OlsrHelper from another previously initialized instance
   * (Copy Constructor).
   *
   * \param o object to copy
   */
  WsngrHelper (const WsngrHelper &o);

  /**
   * \returns pointer to clone of this OlsrHelper
   *
   * This method is mainly for internal use by the other helpers;
   * clients are expected to free the dynamic memory allocated by this method
   */
  WsngrHelper* Copy (void) const;

  /**
   * \param node the node on which the routing protocol will run
   * \returns a newly-created routing protocol
   *
   * This method will be called by ns3::InternetStackHelper::Install
   */
  virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;

  /**
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set.
   *
   * This method controls the attributes of ns3::olsr::RoutingProtocol
   */
  void Set (std::string name, const AttributeValue &value);
  void Install (NodeContainer c) const;

private:
  /**
   * \brief Assignment operator declared private and not implemented to disallow
   * assignment and prevent the compiler from happily inserting its own.
   * \return nothing
   */
  WsngrHelper &operator = (const WsngrHelper &);
  ObjectFactory m_agentFactory; //!< Object factory

  std::map< Ptr<Node>, std::set<uint32_t> > m_interfaceExclusions; //!< container of interfaces excluded from OLSR operations
};
}

#endif /* WSNGR_HELPER_H */

