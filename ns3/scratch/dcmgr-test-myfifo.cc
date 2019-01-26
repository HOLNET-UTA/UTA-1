#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>

#define DCTCP_MODEL 0
#define D2TCP_MODEL 1
#define DCMGR_MODEL 2
#define MGR_MODEL 3

#define WEBSEARCH 0
#define DATAMINING 1
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("dcmgrFifoTest");

//topology
uint32_t num_spines, num_leafs, num_hosts_per_leaf;
NodeContainer spines, leafnodes, hosts, allnodes;
uint16_t *ports;
uint32_t traffic_type;  //0: Web search; 1: data mining
uint32_t load;
uint32_t seed;

// attributes
std::string fabric_datarate, edge_datarate;
std::string fabric_delay, edge_delay;
uint32_t use_model; //0: DCTCP, 1: D2TCP, 2:DCMGR, 3:MGR
double rcos;
double m_g;
uint32_t tcp_rwndmax;
uint32_t initial_ssh;
uint32_t maxCwnd;
uint32_t *queue_map;

double medium_offset;
double medium_speed;
double big_offset;
double big_speed;

// queue params
uint32_t packet_size;
uint32_t fabric_queue_size, edge_queue_size;
double fabric_threshold, edge_threshold;
uint32_t queue_type; //0: pfifo_first, 1: my_fifo

// The times
Time global_start_time;
Time global_stop_time;
Time flow_stop_time;


// nodes
NodeContainer srcs;
NodeContainer routers;
NodeContainer dsts;

// dst interfaces
Ipv4InterfaceContainer dst_interfaces;

// receive status
//bool writeThroughput;
// std::map<uint32_t, uint64_t> totalRx;   //fId->receivedBytes
// std::map<uint32_t, uint64_t> lastRx;
// // throughput result
// std::map<uint32_t, std::vector<std::pair<double, double> > > throughputResult; //fId -> list<time, throughput>

void
SocketCreateTrace (uint64_t flowSize, Time deadline, Ptr<Socket> socket)
{
  socket->SetAttribute ("TotalBytes", UintegerValue (flowSize));
  socket->SetAttribute ("Deadline", TimeValue (deadline));
  //socket->SetAttribute ("InitialCwnd", UintegerValue(2)); //set initial Cwnd to 2;
}

void 
SocketCloseTrace (uint32_t source_node, Ptr<Socket> socket)
{
  uint32_t index = socket->GetMyFifoQueueIndex();
  //NS_LOG_INFO ("release fifo number: " << index << "old fifo map: " << std::hex << queue_map[source_node]);
  queue_map[source_node] &= ~(0x1 << (index-1));
  //NS_LOG_INFO ("new fifo map: " << std::hex << queue_map[source_node]);
}

// void
// TxTrace (uint32_t flowId, Ptr<const Packet> p)
// {
//   NS_LOG_FUNCTION (flowId << p);
//   FlowIdTag flowIdTag;
//   flowIdTag.SetFlowId (flowId);
//   p->AddByteTag (flowIdTag);
// }

// void
// RxTrace (Ptr<const Packet> packet, const Address &from)
// {
//   NS_LOG_FUNCTION (packet << from);
//   FlowIdTag flowIdTag;
//   bool retval = packet->FindFirstMatchingByteTag (flowIdTag);
//   NS_ASSERT (retval);
//   if (totalRx.find (flowIdTag.GetFlowId ()) != totalRx.end ())
//     {
//       totalRx[flowIdTag.GetFlowId ()] += packet->GetSize ();
//     }
//   else
//     {
//       totalRx[flowIdTag.GetFlowId ()] = packet->GetSize ();
//       lastRx[flowIdTag.GetFlowId ()] = 0;
//     }
//   NS_LOG_DEBUG (Simulator::Now () << ", " << flowIdTag.GetFlowId () << ", " << totalRx[flowIdTag.GetFlowId ()]);
// }

// void
// CalculateThroughput (void)
// {
//   for (auto it = totalRx.begin (); it != totalRx.end (); ++it)
//     {
//       double cur = (it->second - lastRx[it->first]) * (double) 8/1e5; /* Convert Application RX Packets to MBits. */
//       throughputResult[it->first].push_back (std::pair<double, double> (Simulator::Now ().GetSeconds (), cur));
//       lastRx[it->first] = it->second;
//     }
//   Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
// }
// void
// SetupName (const NodeContainer& nodes, const std::string& prefix)
// {
//   int i = 0;
//   for(auto it = nodes.Begin (); it != nodes.End (); ++it)
//     {
//       std::stringstream ss;
//       ss << prefix << i++;
//       Names::Add (ss.str (), *it);
//     }
// }

// void
// SetupName (void)
// {
//   SetupName (srcs, "src");
//   SetupName (routers, "router");
//   SetupName (dsts, "dst");
// }

void printlink(Ptr<Node> n1, Ptr<Node> n2)
{
  std::cout<<"printlink: link setup between node "<<n1->GetId()<<" and node "<<n2->GetId()<<std::endl;
} 

void createTopology(void)
{
  NS_LOG_DEBUG("Creating "<<num_spines<<" spines "<<num_leafs<<" leaves "<<num_hosts_per_leaf<<" hosts  per leaf ");
  hosts.Create(num_hosts_per_leaf*num_leafs);
  leafnodes.Create(num_leafs);
  spines.Create(num_spines);
  
  ports = new uint16_t [hosts.GetN()];
  queue_map = new uint32_t [hosts.GetN()];

  for (uint32_t i=0; i <hosts.GetN(); i++) 
    {
      ports[i] = 0;
      queue_map[i] = 0;

    }

  allnodes = NodeContainer (hosts,  leafnodes, spines);
  InternetStackHelper internet;
  internet.Install (allnodes);

  // We create the channels first without any IP addressing information
  // Queue, Channel and link characteristics
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper fabric_link;
  fabric_link.SetQueue ("ns3::DropTailQueue");  // we refer to dctcp-test
  fabric_link.SetDeviceAttribute ("DataRate", DataRateValue (DataRate(fabric_datarate)));
  fabric_link.SetChannelAttribute ("Delay", TimeValue(Time(fabric_delay)));
  TrafficControlHelper fabric_red;
  fabric_red.SetRootQueueDisc ("ns3::RedQueueDisc",
                                "LinkBandwidth", DataRateValue (DataRate(fabric_datarate)),
                                "LinkDelay", TimeValue(Time(fabric_delay)),
                                "MinTh", DoubleValue(fabric_threshold),
                                "MaxTh", DoubleValue(fabric_threshold),
                                "QueueLimit", UintegerValue(fabric_queue_size)
                                );

  PointToPointHelper edge_link;
  edge_link.SetQueue ("ns3::DropTailQueue");  //host doesn't possess a redqueue
  edge_link.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (edge_datarate)));
  edge_link.SetChannelAttribute ("Delay", TimeValue(Time(edge_delay)));
  

  TrafficControlHelper edge_red;
  edge_red.SetRootQueueDisc ("ns3::RedQueueDisc",
                                "LinkBandwidth", DataRateValue (DataRate(edge_datarate)),
                                "LinkDelay", TimeValue(Time(edge_delay)),
                                "MinTh", DoubleValue(edge_threshold),
                                "MaxTh", DoubleValue(edge_threshold),
                                "QueueLimit", UintegerValue(edge_queue_size)
                                );
  TrafficControlHelper host_fifo;
  host_fifo.SetRootQueueDisc ("ns3::MyFifoQueueDisc");

  Ipv4AddressHelper ipv4AddrHelper;
  ipv4AddrHelper.SetBase ("10.1.0.0", "255.255.255.0");
  // Create links between all sourcenodes and bottleneck switch
  //
  std::vector<NetDeviceContainer> fabric_devs;
  std::vector<NetDeviceContainer> edge_devs;

  NS_LOG_DEBUG("Creating edge links..");
  for(uint32_t lidx = 0; lidx < leafnodes.GetN(); lidx++) 
    {
      uint32_t start_index = lidx * num_hosts_per_leaf;
      for(uint32_t hidx = start_index; hidx < start_index + num_hosts_per_leaf; hidx++) 
        {
          NetDeviceContainer net_dev = edge_link.Install(leafnodes.Get(lidx), hosts.Get(hidx));
          edge_devs.push_back(net_dev);
          //edge_red.Install(net_dev);
          for(uint32_t i = 0; i < net_dev.GetN(); i++)
            {
              if (net_dev.Get(i)->GetNode() == leafnodes.Get(lidx))
                {
                  edge_red.Install (net_dev.Get(i));
                }
              if (net_dev.Get(i)->GetNode() == hosts.Get(hidx))
                {
                  host_fifo.Install (net_dev.Get(i));
                }
            }

          ipv4AddrHelper.Assign(net_dev);
          ipv4AddrHelper.NewNetwork ();
          // debug information
          //printlink(leafnodes.Get(lidx), hosts.Get(hidx));
        }
    }

  NS_LOG_DEBUG("Creating fabric links..");
  for(uint32_t sidx = 0; sidx < spines.GetN(); sidx++) 
    {
      for(uint32_t lidx = 0; lidx < leafnodes.GetN(); lidx++) 
        {
          NetDeviceContainer net_dev = fabric_link.Install(spines.Get(sidx), leafnodes.Get(lidx));
          fabric_devs.push_back(net_dev);
          fabric_red.Install (net_dev);
          ipv4AddrHelper.Assign(net_dev);
          ipv4AddrHelper.NewNetwork ();
          // debug information
          //printlink(spines.Get(sidx), leafnodes.Get(lidx));
        }
    }

  // Ptr<NetDevice> device;
  // for(auto x : edge_devs)
  //   {
  //     for(uint32_t i = 0; i < x.GetN(); i++)
  //         {
  //           if (x.Get(i)->GetNode() == hosts.Get(208))
  //             {
  //               device = x.Get(i);
  //               break;
  //             }
  //         }
  //   }
  // edge_link.EnablePcap ("mytest_edge", device);

  //fabric_link.EnablePcapAll ("mytest_fabric");
  //Turn on global static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  // Ptr<OutputStreamWrapper> x = Create<OutputStreamWrapper> (&std::cout);
  // Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt (Simulator::Now(),x);
}

Time getDeadline(uint32_t flow_size)
{
  double dead = 0;
  if (traffic_type == WEBSEARCH)
    {
      if (flow_size < 100000) //100K
        dead = 0.002;
      else if (flow_size <= 1000000) //1MB
        dead = 0.002 + flow_size * 8.0/ (2.0 * 1e9);
      else
        dead = flow_size* 8.0 / (2.0*1e9);
    }
  else if (traffic_type == DATAMINING)
    {
      if (flow_size < 100000) //100K
        dead = 0.001;
       else if (flow_size <= 1000000) //1MB
        dead = medium_offset + flow_size * 8.0/ (medium_speed * 1e9);
      else
        dead = big_offset + flow_size* 8.0 / (big_speed * 1e9);
    }
  else{}

  return Seconds(dead);
}

uint32_t getFlowSize(uint32_t type)
{
  double num = 0;
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  double x = uv->GetValue(0,1);
  double xx = uv->GetValue(0,1);
  if (traffic_type == WEBSEARCH) //Web search distribution unit(kB)
    {
      if (x < 0.3)num = 1 + int(9 * xx);
      else if (x < 0.6) num = 10 + int(90 * xx);
      else if (x < 0.75) num = 100 + int(900 * xx);
      else if (x < 0.99) num = 1000 + int(9000 * xx);
      else  num = 10000 + int(400000 * xx);
    }
  else if (traffic_type == DATAMINING) //data mining distribution
    {
      if (x < 0.4)num = 0.1 + (0.9 * xx);
      else if (x < 0.75) num = 1 + int(9 * xx);
      else if (x < 0.85) num = 10 + int(990 * xx);
      else if (x < 0.99) num = 1000 + int(9000 * xx);
      else  num = 10000 + int(400000 * xx);
    }
  else
    {
      std::cout << "unknown traffic flow type! " << std::endl;
      exit(1);
    }
  return num*1000; //return bytes
}

void startFlow(uint32_t flow_id)
{
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  
  uint32_t source_node = uv->GetInteger(0, hosts.GetN() - 1);  //

  uint8_t queue_index;
  uint32_t map = queue_map[source_node];
  uint i = 0;
  //try to get a host which have empty queue
  for (i = 0; i < hosts.GetN(); i++)//my fifo must assure the source node doesn't have more than 30 flows at the same time
    {
      if (map == 0x1FFFFFFF)
        {
          source_node = (source_node+1) % hosts.GetN();
          map =  queue_map[source_node];
        }
    }

  //get queue index
  if (i == hosts.GetN() && map == 0x1FFFFFFF) //all nodes have full queues
    {
      queue_index = uv->GetInteger(0, 29);
    }
  else
    {
      for (uint8_t i = 1; i <= 29; i++)
        {
          uint32_t bit = map & 0x1;
          if (bit == 0) //find a empty queue
            {
              NS_LOG_DEBUG("find empty queue:" << uint32_t(i));
              queue_index = i;
              queue_map[source_node] |= 0x1 << (i-1);
              break;
            }
          map = map>>1;     
        }
    }
  //NS_LOG_INFO ("new fifo map " << std::hex << queue_map[source_node]);

  uint32_t rack = (source_node/num_hosts_per_leaf) + uv->GetInteger(1, num_leafs-1);  //??
  rack = rack%num_leafs;
  uint32_t sink_node = rack*num_hosts_per_leaf + uv->GetInteger(0, num_hosts_per_leaf - 1);
  uint32_t flow_size = getFlowSize(traffic_type);
   
  //test
  // source_node = flow_id;
  // sink_node =40;
  // flow_size = 10000000;

  Time deadline = Time(0);
  if (flow_id%5 == 0)
    deadline = getDeadline(flow_size);   
  uint16_t port = ++ports[sink_node];
  NS_LOG_INFO ("flow id: " << flow_id << " src: " << source_node << " dst: " << sink_node << "flow_start_time:" << Simulator::Now().GetNanoSeconds() << "ms." << " flow size: " << flow_size << " deadline: " << deadline.GetSeconds() << "s");
 
  //sink app set up
  Ptr<MySinkApp> SinkingApp = CreateObject<MySinkApp> ();
  SinkingApp->SetAttribute("Protocol",  TypeIdValue (TcpSocketFactory::GetTypeId ()));
  SinkingApp->SetAttribute("Local", AddressValue(InetSocketAddress(Ipv4Address::GetAny (), port)));
  SinkingApp->SetAttribute("FlowSize", UintegerValue (flow_size));
  
  SinkingApp->SetStartTime (Time(0));
  SinkingApp->SetStopTime (global_stop_time); 
  hosts.Get(sink_node)->AddApplication(SinkingApp);

  // PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  // ApplicationContainer sinkApps = sinkHelper.Install (hosts.Get(sinkN));
  // sinkApps.Start (flow_start);
  // sinkApps.Stop (global_stop_time);  //
  //trace receive 
  // for (auto it = sinkApps.Begin (); it != sinkApps.End (); ++it)
  //   {
  //     (*it)->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTrace));
  //   }

  //source app set up
  // Ptr<Ipv4L3Protocol> source_node_ipv4 = StaticCast<Ipv4L3Protocol> ((hosts.Get(sourceN))->GetObject<Ipv4> () ); 
  // Ipv4Address sourceIp = source_node_ipv4->GetAddress (1,0).GetLocal();
  // Address sourceAddress = (InetSocketAddress (sourceIp, port));
  // NS_LOG_DEBUG("sourceAddress: " << sourceAddress);
  Ptr<Ipv4L3Protocol> sink_node_ipv4 = StaticCast<Ipv4L3Protocol> (hosts.Get(sink_node)->GetObject<Ipv4> ());
  Ipv4Address remoteIp = sink_node_ipv4->GetAddress (1,0).GetLocal(); //?
  Ptr<MySendApp> SendingApp = CreateObject<MySendApp> ();
  SendingApp->SetAttribute("Remote", AddressValue(InetSocketAddress(remoteIp, port)));
  SendingApp->SetAttribute("FlowSize", UintegerValue (flow_size));
  SendingApp->SetAttribute("SrcNode", PointerValue (hosts.Get(source_node)));
  SendingApp->SetAttribute("SinkNode", PointerValue (hosts.Get(sink_node)));
  SendingApp->SetAttribute("FlowId", UintegerValue (flow_id));
  SendingApp->SetAttribute("Deadline", TimeValue(deadline));
  SendingApp->SetAttribute("QueueIndex", UintegerValue(queue_index));
  SendingApp->SetStartTime(Time(0));
  SendingApp->SetStopTime(global_stop_time);   // need reviewing
  hosts.Get(source_node)->AddApplication(SendingApp);
  // trace socket create
  //SendingApp->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, flow_id));
  if (use_model == D2TCP_MODEL || use_model == DCMGR_MODEL || use_model == MGR_MODEL)
    {
      SendingApp->TraceConnectWithoutContext ("SocketCreate", MakeBoundCallback (&SocketCreateTrace, flow_size, deadline));
    }
  SendingApp->TraceConnectWithoutContext("SocketClose", MakeBoundCallback(&SocketCloseTrace, source_node));

  
  // Ptr<Ipv4L3Protocol> ipv4 = StaticCast<Ipv4L3Protocol> ((sourceNodes.Get(sourceN))->GetObject<Ipv4> ()); // Get Ipv4 instance of the node
  // Ipv4Address addr = ipv4->GetAddress (1, 0).GetLocal();
  //NS_LOG_DEBUG("flow id: " << flow_id << " source node: " <<  sourceN << " sink node: " << sinkN << " start time: " << flow_start <<" deadline: " << deadline);
}



double getAvgInterval()
{
  double avg_interarrival = 100; //ms
  std::cout << "traffic type " << traffic_type << std::endl;
  if (num_hosts_per_leaf == 24)
    {
      if (traffic_type == WEBSEARCH)
        {
          switch (load)
            {            
              case 30: avg_interarrival = 1.0/7669; break;
              case 40: avg_interarrival = 1.0/10225; break;
              case 50: avg_interarrival = 1.0/12781; break;
              case 60: avg_interarrival = 1.0/15338; break;
              case 70: avg_interarrival = 1.0/17894; break;
              case 80: avg_interarrival = 1.0/20450; break;
              default: std::cout << "undefined load" << std::endl; exit(2); 
            }
        }
      else if (traffic_type == DATAMINING)
        {
          switch (load)
            {                 
              case 30: avg_interarrival = 1.0/9238; break;
              case 40: avg_interarrival = 1.0/12317; break;
              case 50: avg_interarrival = 1.0/15396; break;
              case 60: avg_interarrival = 1.0/18476; break;
              case 70: avg_interarrival = 1.0/21555; break;
              case 80: avg_interarrival = 1.0/24634; break;  //?
              default: std::cout << "undefined load" << std::endl; exit(2); 
            }
        }
    } 
  return avg_interarrival; //teturn seconds
}

void setUpTraffic()
{
  global_stop_time = flow_stop_time + Seconds(2);
  Ptr<ExponentialRandomVariable> exp = CreateObject<ExponentialRandomVariable> ();
  double avg_interval = getAvgInterval(); //s
  exp->SetAttribute("Mean", DoubleValue(avg_interval));

  double flow_start_time =global_start_time.GetSeconds();
  uint32_t flow_id = 0;
  
  while (flow_start_time < flow_stop_time.GetSeconds())
  //while (flow_id < 4)
    {  
      Simulator::Schedule (Seconds (flow_start_time) , &startFlow, flow_id);
      //NS_LOG_INFO ("START time: " << flow_start_time);
      flow_start_time = flow_start_time + exp->GetValue();
      flow_id++; 
    }
}

void
SetupConfig (void)
{

  //MySendApp config
  Config::SetDefault ("ns3::MySendApp::PacketSize", UintegerValue (1460));
  Config::SetDefault ("ns3::MySendApp::DataRate", DataRateValue (DataRate (edge_datarate)));
  Config::SetDefault ("ns3::MySendApp::UseMyFifo", BooleanValue (true));

  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));
  // RED params
  NS_LOG_INFO ("Set RED params");
  Config::SetDefault ("ns3::PfifoFastQueueDisc::Limit", UintegerValue (1000));
  Config::SetDefault ("ns3::Queue::MaxPackets", UintegerValue (1));
  Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
  Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (packet_size));
  Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (false));
  Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1.0));
  Config::SetDefault ("ns3::RedQueueDisc::UseMarkP", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::MarkP", DoubleValue (2.0));
  //Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (threshold));
  //Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (threshold));
  //Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (queue_size));

  // enable ECN
  Config::SetDefault ("ns3::TcpSocketBase::UseEcn", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (true));

  // TCP params
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1460));
  Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (Time("10ms")));
  Config::SetDefault ("ns3::TcpSocketBase::MaxCWnd", UintegerValue (maxCwnd));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue(2));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue(500000000));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue(tcp_rwndmax));//512k 524288; 1M 1048576, 128K 131072, 256K 262144
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  Config::SetDefault ("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue (initial_ssh));
  // Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));



  //config ecmp
  Config::SetDefault ("ns3::Ipv4GlobalRouting::EcmpMode", EnumValue(ECMP_HASH));  

  
  if (use_model == DCTCP_MODEL)
    {
      Config::SetDefault ("ns3::DctcpSocket::DctcpWeight", DoubleValue (m_g));
      Config::SetDefault ("ns3::TcpL4Protocol::SocketBaseType", TypeIdValue(TypeId::LookupByName ("ns3::DctcpSocket")));
    }
  else if (use_model == D2TCP_MODEL)
    {
      Config::SetDefault ("ns3::DctcpSocket::DctcpWeight", DoubleValue (m_g));
      Config::SetDefault ("ns3::TcpL4Protocol::SocketBaseType", TypeIdValue(TypeId::LookupByName ("ns3::D2tcpSocket")));
    }
  else if (use_model == DCMGR_MODEL)
    {  
      Config::SetDefault ("ns3::DcmgrSocket::DcmgrWeight", DoubleValue (m_g));
      Config::SetDefault ("ns3::TcpL4Protocol::SocketBaseType", TypeIdValue(TypeId::LookupByName ("ns3::DcmgrSocket")));
      Config::SetDefault ("ns3::DcmgrSocket::Rcos", DoubleValue(rcos));
    }
  else if (use_model == MGR_MODEL)
  {
      // disable ECN
      Config::SetDefault ("ns3::TcpSocketBase::UseEcn", BooleanValue (false));
      fabric_threshold = fabric_queue_size;
      edge_threshold = edge_queue_size;
      Config::SetDefault ("ns3::TcpL4Protocol::SocketBaseType", TypeIdValue(TypeId::LookupByName ("ns3::MgrSocket")));
      Config::SetDefault ("ns3::DcmgrSocket::Rcos", DoubleValue(rcos));
  }

}


CommandLine addCmdOptions(void)
{
  std::string pathOut ("."); // Current directory

  global_start_time = Seconds (0);
  flow_stop_time = Seconds (1);

  num_spines = 6; 
  num_leafs = 6;
  num_hosts_per_leaf = 24;
  load = 80;
  seed = 1;
  traffic_type = 0;
  queue_type = 1; //myfifo

  fabric_datarate = "20Gbps";
  fabric_delay = "30us";
  edge_datarate = "10Gbps";
  edge_delay = "10us";
  packet_size = 1500;  //bytes
  fabric_queue_size = 300; //packet, so as bellow
  fabric_threshold = 130;
  edge_queue_size = 150;
  edge_threshold = 65;
  rcos = 3;
  use_model = DCTCP_MODEL;
  m_g = 1.0 / 16;
  tcp_rwndmax =  262144; //512k 524288; 1M 1048576, 128K 131072, 256K 262144
  initial_ssh = 150000; //initial threshold;
  maxCwnd = 180000;

  medium_offset = 0.001; //1ms
  big_offset = 0; //0ms
  medium_speed = 1; //1Gbps
  big_speed = 1.5; //1.5Gbps  
  CommandLine cmd;
  //the topology
  cmd.AddValue ("useModel", "0: dctcp; 1: d2tcp; 2: dcmgr in test", use_model);
  cmd.AddValue ("packetSize", "the byte lenght of packet in the stystem", packet_size);
  cmd.AddValue ("load", "the traffic load fraction:", load);
  cmd.AddValue ("numSpines", "the number of spins:", num_spines);
  cmd.AddValue ("numLeafs", "the number of leafs:", num_leafs);
  cmd.AddValue ("numHostsPerLeaf", "the number of hosts per leaf:", num_hosts_per_leaf);
  cmd.AddValue ("trafficType", "traffic type, 0: web search 1: data mining", traffic_type);
  cmd.AddValue ("seed", "Random seed", seed);
  cmd.AddValue ("flowStopTime", "flow stop time, unit (s)", flow_stop_time);

  cmd.AddValue ("queueType", "the type of host queue, 0: pfifo; 1: myfifo", queue_type);

  // RED params
  cmd.AddValue ("fabricThreshold", "the packet thread in the queue", fabric_threshold);
  cmd.AddValue ("fabricQueueSize", "the maximun packets in the queue", fabric_queue_size);
  cmd.AddValue ("edgeThreshold", "the packet thread in the queue", edge_threshold);
  cmd.AddValue ("edgeQueueSize", "the maximun packets in the queue", edge_queue_size);

  //dcmgr or dctcp or d2tcp
  cmd.AddValue ("m_g", "the weight of dcmgr of dctcp", m_g);

  cmd.AddValue ("pathOut", "Path to save results from --writeForPlot/--writePcap/--writeFlowMonitor", pathOut);
  cmd.AddValue ("rcos", "increase rate when rwnd < wmin", rcos);

  //deadline
  cmd.AddValue ("MediumOffset", "the deadline offset of medium flow (s)", medium_offset);
  cmd.AddValue ("BigOffset", "the deadline offset of big flow (s)", big_offset);
  cmd.AddValue ("MediumSpeed", "the required speed of medium flow (Gbps)", medium_speed);
  cmd.AddValue ("BigSpeed", "the required speed of medium flow (Gbps)", big_speed);
  return cmd;
}

void printSettings()
{
  std::cout << "fabric_queue_size = " << fabric_queue_size << ", fabric_threshold = " << fabric_threshold << std::endl;
  std::cout << "edge_queue_size = " << edge_queue_size << ", edge_threshold = " << edge_threshold << std::endl;
  std::cout << "medium_offset = " << medium_offset << " s, medium speed = " << medium_speed << " Gbps" << std::endl;
  std::cout << "big_offset = " << big_offset << " s, big speed = " << big_speed << " Gbps" << std::endl;
  std::cout << "flow id,fct,start time,stop time,flow size,deadline,src,dst" << std::endl;
}
int main (int argc, char *argv[])
{
  
  //LogComponentEnable ("DctcpSocket", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("DcmgrSocket", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("MgrSocket", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("Ipv4GlobalRouting", LOG_LEVEL_ERROR);
  //LogComponentEnable ("dcmgrFifoTest", LOG_LEVEL_INFO);
  //LogComponentEnable ("RttEstimator", LOG_LEVEL_FUNCTION);
  //LogComponentEnable ("RedQueueDisc", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("Queue", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("DropTailQueue", LOG_LEVEL_DEBUG);
  LogComponentEnable ("MySendApp", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("MyFifoQueueDisc", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("MySinkApp", LOG_LEVEL_INFO);
  //LogComponentEnable ("TcpSocketBase", LOG_LEVEL_INFO);
  //LogComponentEnable ("TcpCongestionOps", LOG_LEVEL_FUNCTION);

  CommandLine cmd = addCmdOptions();
  cmd.Parse (argc, argv);

  SetupConfig ();
  printSettings();
  //Random seeds
  RngSeedManager::SetSeed(10);
  RngSeedManager::SetRun(seed);

  createTopology();
  //SetupTopo (10, 1, link_data_rate, link_delay);
  
  setUpTraffic();


  std::cout << "simulation start" << std::endl;
  Simulator::Stop (global_stop_time);
  Simulator::Run ();



  Simulator::Destroy ();

  return 0;
}

