#include "mgr-socket.h"
#include "ns3/log.h"
#include "ipv4-end-point.h"
#include "ipv6-end-point.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MgrSocket");

NS_OBJECT_ENSURE_REGISTERED (MgrSocket);

TypeId
MgrSocket::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MgrSocket")
      .SetParent<TcpSocketBase> ()
      .SetGroupName ("Internet")
      .AddConstructor<MgrSocket> ()
      .AddAttribute ("Deadline",
                     "Deadline for current flow.",
                     TimeValue (Time ()),
                     MakeTimeAccessor (&MgrSocket::m_deadline),
                     MakeTimeChecker ())
      .AddAttribute ("TotalBytes",
                     "Total bytes tobe sent.",
                     UintegerValue (0),
                     MakeUintegerAccessor (&MgrSocket::m_totalBytes),
                     MakeUintegerChecker<uint64_t> ())
      .AddAttribute("Rcos", "increase rate when rwnd < wmin",
                     DoubleValue (3),
                     MakeDoubleAccessor (&MgrSocket::m_rcos),
                     MakeDoubleChecker<double> (0))
  ;return tid;
}

TypeId
MgrSocket::GetInstanceTypeId () const
{
  return MgrSocket::GetTypeId ();
}

MgrSocket::MgrSocket (void)
  : TcpSocketBase (),
    m_deadline(0),
    m_finishTime(0),
    m_totalBytes(0),
    m_sentBytes(0),
    m_wmin(0),
    m_maxCWnd(0)
{
}

MgrSocket::MgrSocket (const MgrSocket &sock)
  : TcpSocketBase (sock),
    m_deadline(sock.m_deadline),
    m_finishTime(sock.m_finishTime),
    m_totalBytes(sock.m_totalBytes),
    m_sentBytes(sock.m_sentBytes),
    m_rcos(sock.m_rcos),
    m_wmin(sock.m_wmin),
    m_maxCWnd(sock.m_maxCWnd)
{ 
}


Ptr<TcpSocketBase>
MgrSocket::Fork (void)
{
  return CopyObject<MgrSocket> (this);
}



void
MgrSocket::IncreaseWindow (uint32_t segmentAcked)
{
  NS_LOG_FUNCTION (this << segmentAcked);

  //m_maxCWnd = static_cast<uint32_t> (RTT * 10000000000 / 8); //10Gbps LINK
  double Sf = 0;
  double Td = 0;
  double RTT = 0;

  if (m_deadline != Time (0))
    { //for flows with deadline
      Sf = m_totalBytes - m_sentBytes;
      Td = m_finishTime.GetSeconds () - Simulator::Now ().GetSeconds ();
      RTT = m_rtt->GetEstimate ().GetSeconds ();
      if (Td <= 0)
        {
          m_wmin = m_tcb->m_cWnd.Get();
        }
      else
        {
          m_wmin =  static_cast<uint32_t>(Sf * RTT / Td) ;
        }
    }
  else 
    { //for flows without deadline
      m_wmin = 0;
    }

  if (m_tcb->m_cWnd < m_tcb->m_ssThresh)
    {
      segmentAcked = SlowStart (segmentAcked);
    }

  if (m_tcb->m_cWnd >= m_tcb->m_ssThresh)
    {
      CongestionAvoidance (segmentAcked);
    }


  NS_LOG_DEBUG ("[Node:" << m_node->GetId() << "]" << 
                " Congestion control called (increase window): " <<
                " deadline: " << m_deadline.GetSeconds()*1000 << 
                " cWnd: " << m_tcb->m_cWnd <<
                " Window min: " << m_wmin << 
                " ssTh: " << m_tcb->m_ssThresh << 
                " Sf: " << Sf/1000000 << "MB" 
                " Td: " << Td*1000 <<  "ms"
                " RTT: " << RTT*1000 << "ms"
                  );
}

uint32_t
MgrSocket::SlowStart (uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << m_tcb << segmentsAcked);
  NS_LOG_INFO ("In SlowStart of Mgr, previous cwnd " << m_tcb->m_cWnd.Get() << " wmin " << m_wmin);
  if (segmentsAcked >= 1)
    {
      NS_LOG_INFO ("Segmentacked: " << segmentsAcked);
      uint32_t cWnd = m_tcb->m_cWnd.Get();
      if (cWnd < m_wmin)
        {
          cWnd += (2 * m_rcos - 1) * m_tcb->m_segmentSize * segmentsAcked; //increase acked size
          if (cWnd > m_tcb->m_ssThresh)
            {
              cWnd = m_tcb->m_ssThresh + 1;       
            }
          segmentsAcked -= (cWnd - m_tcb->m_cWnd) / (m_tcb->m_segmentSize * 2 * (m_rcos - 1));
        }
      else
        {
          cWnd += m_tcb->m_segmentSize * segmentsAcked;//increase acked size
          if (cWnd > m_tcb->m_ssThresh)
            {
              cWnd = m_tcb->m_ssThresh + 1;       
            }
          segmentsAcked -= (cWnd - m_tcb->m_cWnd) / (m_tcb->m_segmentSize);
        }
      m_tcb->m_cWnd = cWnd;
      NS_LOG_INFO ("In SlowStart of Mgr, updated to cwnd " << m_tcb->m_cWnd.Get() << " ssthresh " << m_tcb->m_ssThresh); 
    }
  return segmentsAcked;
}


void
MgrSocket::CongestionAvoidance (uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << m_tcb << segmentsAcked);
  NS_LOG_INFO ("In CongAvoid of Mgr, previous cwnd " << m_tcb->m_cWnd.Get() <<
                  " wmin " << m_wmin);
  uint32_t cWnd = m_tcb->m_cWnd.Get();
  if (segmentsAcked > 0)
    {
      if (cWnd < m_wmin)
        {
          double adder = static_cast<double> (m_tcb->m_segmentSize * m_tcb->m_segmentSize) * segmentsAcked / m_tcb->m_cWnd.Get (); //increase acked size
          adder = std::max (1.0, adder);
          cWnd += static_cast<uint32_t> ((m_rcos - 1) * m_tcb->m_segmentSize + adder);
        }
      else  
        {
          double adder = static_cast<double> (m_tcb->m_segmentSize * m_tcb->m_segmentSize) * segmentsAcked / m_tcb->m_cWnd.Get (); //increase acked size
          adder = std::max (1.0, adder);
          cWnd += static_cast<uint32_t> (adder);
        }
      m_tcb->m_cWnd = cWnd;
      NS_LOG_INFO ("In CongAvoid of Mgr, updated to cwnd " << m_tcb->m_cWnd.Get() <<
                   " ssthresh " << m_tcb->m_ssThresh);
    }
}


int
MgrSocket::Connect (const Address &address)
{
  NS_LOG_FUNCTION (this << address);

  // If haven't do so, Bind() this socket first
  if (InetSocketAddress::IsMatchingType (address) && m_endPoint6 == 0)
    {
      if (m_endPoint == 0)
        {
          if (Bind () == -1)
            {
              NS_ASSERT (m_endPoint == 0);
              return -1; // Bind() failed
            }
          NS_ASSERT (m_endPoint != 0);
        }
      InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
      m_endPoint->SetPeer (transport.GetIpv4 (), transport.GetPort ());
      SetIpTos (transport.GetTos ());
      m_endPoint6 = 0;
      
      // Get the appropriate local address and port number from the routing protocol and set up endpoint
      if (SetupEndpoint () != 0)
        {
          NS_LOG_ERROR ("Route to destination does not exist ?!");
          return -1;
        }
    }
  else if (Inet6SocketAddress::IsMatchingType (address)  && m_endPoint == 0)
    {
      // If we are operating on a v4-mapped address, translate the address to
      // a v4 address and re-call this function
      Inet6SocketAddress transport = Inet6SocketAddress::ConvertFrom (address);
      Ipv6Address v6Addr = transport.GetIpv6 ();
      if (v6Addr.IsIpv4MappedAddress () == true)
        {
          Ipv4Address v4Addr = v6Addr.GetIpv4MappedAddress ();
          return Connect (InetSocketAddress (v4Addr, transport.GetPort ()));
        }

      if (m_endPoint6 == 0)
        {
          if (Bind6 () == -1)
            {
              NS_ASSERT (m_endPoint6 == 0);
              return -1; // Bind() failed
            }
          NS_ASSERT (m_endPoint6 != 0);
        }
      m_endPoint6->SetPeer (v6Addr, transport.GetPort ());
      m_endPoint = 0;

      // Get the appropriate local address and port number from the routing protocol and set up endpoint
      if (SetupEndpoint6 () != 0)
        { // Route to destination does not exist
          return -1;
        }
    }
  else
    {
      m_errno = ERROR_INVAL;
      return -1;
    }

  // Re-initialize parameters in case this socket is being reused after CLOSE
  m_rtt->Reset ();
  m_synCount = m_synRetries;
  m_dataRetrCount = m_dataRetries;

  m_finishTime = m_deadline != Time (0) ? Simulator::Now () + m_deadline : Time (0);

  // DoConnect() will do state-checking and send a SYN packet
  return DoConnect ();
}

void
MgrSocket::UpdateRttHistory (const SequenceNumber32 &seq, uint32_t sz, bool isRetransmission)
{
  NS_LOG_FUNCTION (this);

  // set dcmgr max seq to highTxMark
  //m_dcmgrMaxSeq =std::max (std::max (seq + sz, m_tcb->m_highTxMark.Get ()), m_dcmgrMaxSeq);

  // update the history of sequence numbers used to calculate the RTT
  if (isRetransmission == false)
    { // This is the next expected one, just log at end
      m_history.push_back (RttHistory (seq, sz, Simulator::Now ()));
      m_sentBytes += sz;
    }
  else
    { // This is a retransmit, find in list and mark as re-tx
      for (RttHistory_t::iterator i = m_history.begin (); i != m_history.end (); ++i)
        {
          if ((seq >= i->seq) && (seq < (i->seq + SequenceNumber32 (i->count))))
            { // Found it
              i->retx = true;

              m_sentBytes -= i->count;
              i->count = ((seq + SequenceNumber32 (sz)) - i->seq); // And update count in hist
              m_sentBytes += i->count;
              break;
            }
        }
    }
}

} // namespace ns3
