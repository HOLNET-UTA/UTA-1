#include "dcmgr-socket-factory.h"

#include "dcmgr-socket.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (DcmgrSocketFactory);

TypeId
DcmgrSocketFactory::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DcmgrSocketFactory")
      .SetParent<DcmgrSocketFactoryBase> ()
      .SetGroupName ("Internet")
      .AddConstructor<DcmgrSocketFactory> ();
  return tid;
}

Ptr<Socket>
DcmgrSocketFactory::CreateSocket (void)
{
  TypeIdValue congestionTypeId;
  GetTcp ()->GetAttribute ("SocketType", congestionTypeId);  //we shold set socketType to DcmgrCongestion 
  Ptr<Socket> socket = GetTcp ()->CreateSocket (congestionTypeId.Get (), DcmgrSocket::GetTypeId ());
  socket->SetAttribute ("UseEcn", BooleanValue (true));
  return socket;
}

} // namespace ns3
