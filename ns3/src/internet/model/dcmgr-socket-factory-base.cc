#include "dcmgr-socket-factory-base.h"

#include "ns3/assert.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (DcmgrSocketFactoryBase);

TypeId
DcmgrSocketFactoryBase::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DcmgrSocketFactoryBase")
      .SetParent<SocketFactory> ()
      .SetGroupName ("Internet");
  return tid;
}

DcmgrSocketFactoryBase::DcmgrSocketFactoryBase ()
  : m_tcp (0)
{
}

DcmgrSocketFactoryBase::~DcmgrSocketFactoryBase ()
{
  NS_ASSERT (m_tcp == 0);
}

void
DcmgrSocketFactoryBase::SetTcp (Ptr<TcpL4Protocol> tcp)
{
  m_tcp = tcp;
}

Ptr<TcpL4Protocol>
DcmgrSocketFactoryBase::GetTcp (void)
{
  return m_tcp;
}

void
DcmgrSocketFactoryBase::DoDispose (void)
{
  m_tcp = 0;
  SocketFactory::DoDispose ();
}

} // namespace ns3
