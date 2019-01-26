#ifndef MY_SENDING_APP_H
#define MY_SENDING_APP_H
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

namespace ns3
{
  class MySendApp : public Application
  {
  public:
    static TypeId GetTypeId (void);
    MySendApp ();
    virtual ~MySendApp ();
    virtual void StartApplication (void);
    uint32_t getFlowId(void);
    virtual void StopApplication (void);
    bool keepSending(void);
    void HandleClose (Ptr<Socket> socket);
    void HandleErrorClose (Ptr<Socket> socket);
    void AckChange (SequenceNumber32 oldAck, SequenceNumber32 newAck);
    void CwndChange(uint32_t oldValue, uint32_t newValue);
    void RtoChange (Time oldRto, Time newRto);
    typedef void (* SocketTracedCallback) (Ptr<Socket> socket);

  private:
    void ScheduleTx (void);
    void SendPacket (void);
    Ptr<Socket>     m_socket;
    Address         m_peer;
    uint32_t        m_packetSize;
    DataRate        m_dataRate;  //bits
    EventId         m_sendEvent;
    bool            m_running;
    uint32_t        m_packetsSent;
    uint32_t        m_maxBytes;
    //double        m_startTime;
    //double        m_stoptime;
    EventId         m_startEvent;
    uint32_t        m_totBytes;
    //Address       myAddress;
    Ptr<Node>       srcNode;
    Ptr<Node>       destNode;
    TypeId          m_tid;          
    uint32_t        m_fid;
    TracedCallback<Ptr<const Packet> > m_txTrace;
    int64_t         m_real_start;
    int64_t         m_real_stop;
    Time            m_deadline;

    bool    m_useMyFifo;
    uint8_t m_queueIndex;


    TracedCallback<Ptr<Socket> > m_socketCreateTrace;
    TracedCallback<Ptr<Socket> > m_socketCloseTrace;
  };
}

#endif //

