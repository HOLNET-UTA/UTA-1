#ifndef MGR_SOCKET_FACTORY_H
#define MGR_SOCKET_FACTORY_H

#include "mgr-socket-factory-base.h"

namespace ns3 {

class MgrSocketFactory : public MgrSocketFactoryBase
{
public:
  /**
   * Get the type ID.
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  virtual Ptr<Socket> CreateSocket (void);

};

} // namespace ns3

#endif // MGR_SOCKET_FACTORY_H
