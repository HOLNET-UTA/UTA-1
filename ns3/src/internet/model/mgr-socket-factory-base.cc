#include "mgr-socket-factory-base.h"

#include "ns3/assert.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (MgrSocketFactoryBase);

TypeId
MgrSocketFactoryBase::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MgrSocketFactoryBase")
      .SetParent<SocketFactory> ()
      .SetGroupName ("Internet");
  return tid;
}

MgrSocketFactoryBase::MgrSocketFactoryBase ()
  : m_tcp (0)
{
}

MgrSocketFactoryBase::~MgrSocketFactoryBase ()
{
  NS_ASSERT (m_tcp == 0);
}

void
MgrSocketFactoryBase::SetTcp (Ptr<TcpL4Protocol> tcp)
{
  m_tcp = tcp;
}

Ptr<TcpL4Protocol>
MgrSocketFactoryBase::GetTcp (void)
{
  return m_tcp;
}

void
MgrSocketFactoryBase::DoDispose (void)
{
  m_tcp = 0;
  SocketFactory::DoDispose ();
}

} // namespace ns3
