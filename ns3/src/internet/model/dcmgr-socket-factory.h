#ifndef DCMGR_SOCKET_FACTORY_H
#define DCMGR_SOCKET_FACTORY_H

#include "dcmgr-socket-factory-base.h"

namespace ns3 {

class DcmgrSocketFactory : public DcmgrSocketFactoryBase
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

#endif // DCTCP_SOCKET_FACTORY_H
