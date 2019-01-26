#include "sending_app.h"


namespace ns3 
{
  NS_LOG_COMPONENT_DEFINE ("MySendApp");
 
  NS_OBJECT_ENSURE_REGISTERED (MySendApp);

  TypeId MySendApp::GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::MySendApp")
      .SetParent<Application> ()
      .SetGroupName("Applications") 
      .AddConstructor<MySendApp> ()
      .AddAttribute ("PacketSize", "The amount of data to send each time.",
                      UintegerValue (1024),
                      MakeUintegerAccessor (&MySendApp::m_packetSize),
                      MakeUintegerChecker<uint32_t> (1))
      .AddAttribute ("Remote", "The address of the destination",
                      AddressValue (),
                      MakeAddressAccessor (&MySendApp::m_peer),
                      MakeAddressChecker ())
      .AddAttribute ("FlowSize",
                      "The total number of bytes in this flow. ",
                      UintegerValue (100),
                      MakeUintegerAccessor (&MySendApp::m_maxBytes),
                      MakeUintegerChecker<uint64_t> (0))
      .AddAttribute ("Protocol", "The type of protocol to use.",
                      TypeIdValue (TcpSocketFactory::GetTypeId ()),
                      MakeTypeIdAccessor (&MySendApp::m_tid),
                      MakeTypeIdChecker ())
      .AddAttribute ("SrcNode", "The pointer to the source node.",
                      PointerValue(0),
                      MakePointerAccessor (&MySendApp::srcNode),
                      MakePointerChecker <Node>() )
      .AddAttribute ("SinkNode", "The pointer to the source node.",
                      PointerValue(0),
                      MakePointerAccessor (&MySendApp::destNode),
                      MakePointerChecker <Node>() )
      .AddAttribute ("DataRate", "The link rate (bits)",
                      DataRateValue(DataRate("10Gbps")),
                      MakeDataRateAccessor (&MySendApp::m_dataRate),
                      MakeDataRateChecker ())
      .AddAttribute ("Deadline", "The deadline",
                      TimeValue(Time(0)),
                      MakeTimeAccessor (&MySendApp::m_deadline),
                      MakeTimeChecker ())
      .AddAttribute ("UseMyFifo", "True to use my-fifo-queue-disc",
                      BooleanValue (false),
                      MakeBooleanAccessor (&MySendApp::m_useMyFifo),
                      MakeBooleanChecker ())
      .AddAttribute ("QueueIndex",
                      "The total number of bytes in this flow. ",
                      UintegerValue (0),
                      MakeUintegerAccessor (&MySendApp::m_queueIndex),
                      MakeUintegerChecker<uint64_t> (0))
      .AddAttribute ("FlowId",
                      "The flow Id ",
                      UintegerValue (0),
                      MakeUintegerAccessor (&MySendApp::m_fid),
                      MakeUintegerChecker<uint32_t> (0))
      .AddTraceSource ("Tx", "A new packet is created and is sent",
                        MakeTraceSourceAccessor (&MySendApp::m_txTrace),
                        "ns3::Packet::TracedCallback")
      .AddTraceSource ("SocketCreate", "Socket is created.",
                     MakeTraceSourceAccessor (&MySendApp::m_socketCreateTrace),
                     "ns3::MySendApplication::SocketTracedCallback")
      .AddTraceSource ("SocketClose", "Socket is created.",
                     MakeTraceSourceAccessor (&MySendApp::m_socketCloseTrace),
                     "ns3::MySendApplication::SocketTracedCallback")
    ;
    return tid;
  }
  

  MySendApp::MySendApp ()
    : m_socket (0),
      m_running (false),
      m_totBytes (0),
      m_deadline(Time(0))
  {
      NS_LOG_FUNCTION (this);
  }

  MySendApp::~MySendApp ()
  {
     NS_LOG_FUNCTION (this);
    m_socket = 0;
  }

  uint32_t
  MySendApp::getFlowId(void)
  {
    return m_fid;
  }

  void
  MySendApp::StartApplication (void)
  {
    NS_LOG_FUNCTION (this);
    NS_LOG_INFO ("StartApplication for fid "<<m_fid<<" called at "<<Simulator::Now().GetSeconds()); 

    // Create the socket if not already
    if (!m_socket)
    {
      m_running = true;
      m_totBytes = 0;
      m_socket = Socket::CreateSocket (srcNode, m_tid);
      m_socketCreateTrace (m_socket);

      if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
          if (m_socket->Bind6 () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
        }
      else if (InetSocketAddress::IsMatchingType (m_peer))
        {
          if (m_socket->Bind () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
        }
    }
    if (m_useMyFifo)
      {
        m_socket->setMyFifoQueueIndex(m_queueIndex);
      }
    m_socket->Connect (m_peer);
    //SET end trace
    m_socket->SetCloseCallbacks(MakeCallback (&MySendApp::HandleClose, this),
                                MakeCallback (&MySendApp::HandleErrorClose, this));
    
    
    //uint32_t ecmp_hash_value = Ipv4GlobalRouting::ecmp_hash(tuplevalue);
    
    NS_LOG_INFO("flow_start: "<<m_fid<<" start time: "<<Simulator::Now().GetNanoSeconds()<< " deadline: " << m_deadline.GetNanoSeconds() <<" flow size: "<<m_maxBytes<<" "<<srcNode->GetId()<<" "<<destNode->GetId());
    
    //m_socket->TraceConnectWithoutContext ("HighestRxAck", MakeCallback (&MySendApp::AckChange,this));
    //m_socket->TraceConnectWithoutContext ("RTO", MakeCallback(&MySendApp::RtoChange,this));
    //m_socket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&MySendApp::CwndChange, this));

    m_real_start = Simulator::Now().GetNanoSeconds();
    SendPacket ();
    //FlowData dt(m_fid, m_maxBytes, flow_known, srcNode->GetId(), destNode->GetId(), fweight);
    //flowTracker->registerEvent(1);
  }

  void MySendApp::CwndChange(uint32_t oldValue, uint32_t newValue)
  {
    NS_LOG_DEBUG("flow_id" << m_fid << "old cwnd: " << oldValue <<" new cwnd: " << newValue);
  }

  void MySendApp::RtoChange(Time oldRto, Time newRto)
  {
    NS_LOG_INFO("flow id: " << m_fid <<" old rto: " << oldRto.GetSeconds() << "new rto" << newRto.GetSeconds());
  }

  void MySendApp::AckChange (SequenceNumber32 oldAck, SequenceNumber32 newAck)
  {
    NS_LOG_FUNCTION (this << newAck);
    //std::cout << "ack" << newAck;
    if (newAck.GetValue() == m_maxBytes+1)
      {
        m_real_stop = Simulator::Now().GetNanoSeconds();
        int64_t fct = m_real_stop - m_real_start;
        NS_LOG_DEBUG (m_fid << "," << 
                      fct << "," <<
                      m_real_start << "," << 
                      m_real_stop << "," <<
                      m_maxBytes << "," <<
                      m_deadline.GetNanoSeconds() << "," <<
                      srcNode->GetId() << "," <<
                      destNode->GetId() 
                      );
         StopApplication();             
      }
  }


  void MySendApp::HandleClose (Ptr<Socket> socket)
  {
    NS_LOG_FUNCTION (this << socket);
    m_real_stop = Simulator::Now().GetNanoSeconds();
    int64_t fct = m_real_stop - m_real_start;
    NS_LOG_DEBUG (m_fid << "," << 
                  fct << "," <<
                  m_real_start << "," << 
                  m_real_stop << "," <<
                  m_maxBytes << "," <<
                  m_deadline.GetNanoSeconds() << "," <<
                  srcNode->GetId() << "," <<
                  destNode->GetId() 
                  );
      StopApplication();  
  }

  void MySendApp::HandleErrorClose (Ptr<Socket> socket)
  {
    NS_LOG_FUNCTION (this << socket);
    m_real_stop = Simulator::Now().GetNanoSeconds();
    int64_t fct = m_real_stop - m_real_start;
    NS_LOG_DEBUG (m_fid << "," << 
                  fct << "," <<
                  m_real_start << "," << 
                  m_real_stop << "," <<
                  m_maxBytes << "," <<
                  m_deadline.GetNanoSeconds() << "," <<
                  srcNode->GetId() << "," <<
                  destNode->GetId() //<< ",Error close"
                  );
    StopApplication();  
  }

  void  MySendApp::StopApplication (void)
  {
    m_running = false;
    if (m_useMyFifo)
      {
        m_socketCloseTrace(m_socket);
      }

    if (m_sendEvent.IsRunning ())
      {
        Simulator::Cancel (m_sendEvent);
      }

    if (m_socket)
      {
        m_socket->Close ();
      }

    // Note: this statement is here and works for the case when we are starting and stopping 
    // same flows 
    // When we have dynamic traffic, the stop has to be declared from the destination
    // So, remove it when using dynamic traffic
  //   std::cout<<"flow_stop "<<m_fid<<" start_time "<<Simulator::Now().GetNanoSeconds()<<" flow_size "<<m_maxBytes<<" "<<srcNode->GetId()<<" "<<destNode->GetId() <<" "<<m_weight<<" "<<ecmp_hash_value<<" "<<std::endl;
    NS_LOG_INFO("flow_stop"<<m_fid << "time" << m_stopTime.GetSeconds() <<" flow size " << m_maxBytes << " "<<srcNode->GetId()<<" "<<destNode->GetId()<<" at "<<(Simulator::Now()).GetNanoSeconds()<<" "<<m_maxBytes<<" port "<< InetSocketAddress::ConvertFrom (m_peer).GetPort ());
  //  Ptr<Ipv4L3Protocol> ipv4 = StaticCast<Ipv4L3Protocol> (srcNode->GetObject<Ipv4> ());
  //  ipv4->addToDropList(m_fid);

    //std::cout<<Simulator::Now().GetSeconds()<<" flowid "<<m_fid<<" stopped sending after sending "<<m_totBytes<<std::endl;
  }

  // bool
  // MySendApp::keepSending(void)
  // {
  //   if(m_stopTime != Time(0) && (Simulator::Now().GetSeconds() >= m_stopTime)) {
  //     NS_LOG_INFO ("stoptime reached "<< m_stopTime.GetNanoSeconds() << "ns" );
  //     return false;
  //   }
  //   if(m_maxBytes != 0 && (m_totBytes >= m_maxBytes)) {
  //     return false;
  //   } 

  //   return true;
  // }
    

  void
  MySendApp::SendPacket (void)
  {
    uint32_t pktsize = m_packetSize;
    uint32_t bytes_remaining = m_maxBytes - m_totBytes;
    if(bytes_remaining < m_packetSize) 
      {
        pktsize = bytes_remaining;
      }
    Ptr<Packet> packet = Create<Packet> (pktsize);

    int actual = m_socket->Send( packet );
    m_txTrace (packet); 
    if (actual > 0)
      {
        m_totBytes += actual;
        NS_LOG_INFO("send packet, flow_id"<<m_fid <<" flow size: " << m_maxBytes  << " packet size: " << actual<< " sent bytes: " << m_totBytes << " at "<<(Simulator::Now()).GetNanoSeconds());
      }
    // We exit this loop when actual < toSend as the send side
    // buffer is full. The "DataSent" callback will pop when
    // some buffer space has freed ip.
    if ((unsigned)actual != pktsize)
      {
        NS_LOG_DEBUG("tcp sender buffer not enough");
        return;
      }
    if ( (m_maxBytes == 0) || m_totBytes < m_maxBytes)
      {
        Time tNext (Seconds ((m_packetSize+40) * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
        m_sendEvent = Simulator::Schedule (tNext, &MySendApp::SendPacket, this);
      }
  }

//   void
//   MySendApp::ScheduleTx (void)
//   {
//     //if (m_running)
//     if ((m_maxBytes == 0) || (m_totBytes < m_maxBytes))
//       {
//         //Time tNext (Seconds ((m_packetSize+38) * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
//         Time tNext (Seconds (1500 * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
//         m_sendEvent = Simulator::Schedule (tNext, &MySendApp::SendPacket, this);
//       } else {
//         StopApplication();
//       }
//   }
}

 
