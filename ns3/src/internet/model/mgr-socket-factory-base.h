#ifndef MGR_SOCKET_FACTORY_BASE_H
#define MGR_SOCKET_FACTORY_BASE_H

#include "tcp-l4-protocol.h"

#include "ns3/socket-factory.h"

namespace ns3 {

class MgrSocketFactoryBase : public SocketFactory
{
public:
  /**
   * Get the type ID.
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  MgrSocketFactoryBase ();

  virtual ~MgrSocketFactoryBase ();

  /**
   * \brief Set the associated TCP L4 protocol.
   * \param tcp the TCP L4 protocol
   */
  void SetTcp (Ptr<TcpL4Protocol> tcp);

  virtual Ptr<Socket> CreateSocket (void) = 0;

protected:

  Ptr<TcpL4Protocol> GetTcp (void);

  virtual void DoDispose (void);

  Ptr<TcpL4Protocol> m_tcp; //!< the associated TCP L4 protocol
};

} // namespace ns3

#endif // Mgr_SOCKET_FACTORY_BASE_H
