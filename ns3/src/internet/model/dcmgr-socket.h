#ifndef DCMGR_SOCKET_H
#define DCMGR_SOCKET_H

#include "tcp-socket-base.h"
//#include "tcp-congestion-ops.h"
#include "tcp-l4-protocol.h"
#include "ns3/object.h"
#include "ns3/timer.h"

namespace ns3 {

class DcmgrSocket : public TcpSocketBase
{
public:
  /**
   * Get the type ID.
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Get the instance TypeId
   * \return the instance TypeId
   */
  virtual TypeId GetInstanceTypeId () const;

  /**
   * Create an unbound TCP socket
   */
  DcmgrSocket (void);

  /**
   * Clone a TCP socket, for use upon receiving a connection request in LISTEN state
   *
   * \param sock the original Tcp Socket
   */
  DcmgrSocket (const DcmgrSocket& sock);

protected:

  // inherited from TcpSocketBase
  virtual void SendACK (void);
  virtual void EstimateRtt (const TcpHeader& tcpHeader);
  virtual void UpdateRttHistory (const SequenceNumber32 &seq, uint32_t sz,
                                 bool isRetransmission);
  virtual void DoRetransmit (void);
  virtual Ptr<TcpSocketBase> Fork (void);
  virtual void UpdateEcnState (const TcpHeader &tcpHeader);
  virtual void IncreaseWindow (uint32_t segmentAcked);
  virtual uint32_t GetSsThresh (void);
  virtual bool MarkEmptyPacket (void) const;
  virtual int Connect (const Address &address);


  void UpdateAlpha (const TcpHeader &tcpHeader);
  uint32_t SlowStart (uint32_t segmentsAcked);
  void CongestionAvoidance (uint32_t segmentsAcked);
  // DCMGR related params
  double m_g;   //!< dcmgr g param
  TracedValue<double> m_alpha;   //!< dcmgr alpha param
  uint32_t m_ackedBytesEcn; //!< acked bytes with ecn
  uint32_t m_ackedBytesTotal;   //!< acked bytes total
  SequenceNumber32 m_alphaUpdateSeq;
  SequenceNumber32 m_dcmgrMaxSeq;
  bool m_ecnTransition; //!< ce state machine to support delayed ACK

  Time                  m_deadline;         //!< deadline of current flow
  Time                  m_finishTime;       //!< actual finish time in real
  uint64_t              m_totalBytes;       //!< total bytes to send
  uint64_t              m_sentBytes;        //!< bytes already sent
  double                m_rcos;             //!< Wcnd increase rate when Wcnd < Wmin
  uint32_t              m_wmin;             //!< min window for tcmgr control, added by zcw 2018.12.5
  uint32_t              m_maxCWnd;
};

} // namespace ns3

#endif // DCMGR_SOCKET_H
