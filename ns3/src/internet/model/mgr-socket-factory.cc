#include "mgr-socket-factory.h"

#include "mgr-socket.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (MgrSocketFactory);

TypeId
MgrSocketFactory::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MgrSocketFactory")
      .SetParent<MgrSocketFactoryBase> ()
      .SetGroupName ("Internet")
      .AddConstructor<MgrSocketFactory> ();
  return tid;
}

Ptr<Socket>
MgrSocketFactory::CreateSocket (void)
{
  TypeIdValue congestionTypeId;
  GetTcp ()->GetAttribute ("SocketType", congestionTypeId);  //we shold set socketType to MgrCongestion 
  Ptr<Socket> socket = GetTcp ()->CreateSocket (congestionTypeId.Get (), MgrSocket::GetTypeId ());
  socket->SetAttribute ("UseEcn", BooleanValue (false));
  return socket;
}

} // namespace ns3
