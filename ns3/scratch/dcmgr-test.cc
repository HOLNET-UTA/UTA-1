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
#define WEBSEARCH 0
#define DATAMINING 1
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("dcmgrTest");

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


// queue params
uint32_t packet_size;
uint32_t fabric_queue_size, edge_queue_size;
double fabric_threshold, edge_threshold;

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
   
  for (uint32_t i=0; i <hosts.GetN(); i++) 
    {
      ports[i] = 1;
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




void startFlow(uint32_t sourceN, uint32_t sinkN, Time flow_start, uint32_t flow_size, uint32_t flow_id, Time deadline)
{
  uint16_t port = ++ports[sinkN];
  Ptr<Ipv4L3Protocol> sink_node_ipv4 = StaticCast<Ipv4L3Protocol> ((hosts.Get(sinkN))->GetObject<Ipv4> ());
  Ipv4Address remoteIp = sink_node_ipv4->GetAddress (1,0).GetLocal(); //?
  InetSocketAddress addr(remoteIp, port);
  //set tos according to flow size
  if (flow_size < 100000) //100kB, small flow
    addr.SetTos(16); //NS3_PRIO_INTERACTIVE
  else if (flow_size < 1000000) //1MB, midium flow
    addr.SetTos(0);//NS3_PRIO_BESTEFFORT
  else //big flow
    addr.SetTos(8);//NS3_PRIO_BULK
  Address remoteAddress = Address(addr);

  // Ptr<Ipv4L3Protocol> source_node_ipv4 = StaticCast<Ipv4L3Protocol> ((hosts.Get(sourceN))->GetObject<Ipv4> () ); 
  // Ipv4Address sourceIp = source_node_ipv4->GetAddress (1,0).GetLocal();
  // Address sourceAddress = (InetSocketAddress (sourceIp, port));
  // NS_LOG_DEBUG("sourceAddress: " << sourceAddress);

  //sink app set up
  InetSocketAddress local(Ipv4Address::GetAny (), port);
  local.SetTos(16); //for ack
  Ptr<MySinkApp> SinkingApp = CreateObject<MySinkApp> ();
  SinkingApp->SetAttribute("Protocol",  TypeIdValue (TcpSocketFactory::GetTypeId ()));
  SinkingApp->SetAttribute("Local", AddressValue(Address(local)));
  SinkingApp->SetAttribute("FlowSize", UintegerValue (flow_size));
  SinkingApp->SetStartTime (flow_start);
  SinkingApp->SetStopTime (global_stop_time); 
  hosts.Get(sinkN)->AddApplication(SinkingApp);

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
  Ptr<MySendApp> SendingApp = CreateObject<MySendApp> ();
  SendingApp->SetAttribute("Remote", AddressValue(remoteAddress));
  SendingApp->SetAttribute("FlowSize", UintegerValue (flow_size));
  SendingApp->SetAttribute("SrcNode", PointerValue (hosts.Get(sourceN)));
  SendingApp->SetAttribute("SinkNode", PointerValue (hosts.Get(sinkN)));
  SendingApp->SetAttribute("FlowId", UintegerValue (flow_id));
  SendingApp->SetAttribute("Deadline", TimeValue(deadline));
  SendingApp->SetStartTime(flow_start);
  SendingApp->SetStopTime(global_stop_time);   // need reviewing
  hosts.Get(sourceN)->AddApplication(SendingApp);
  // trace socket create
  //SendingApp->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, flow_id));
  if (use_model == D2TCP_MODEL || use_model == DCMGR_MODEL)
    {
      SendingApp->TraceConnectWithoutContext ("SocketCreate", MakeBoundCallback (&SocketCreateTrace, flow_size, deadline));
    }
  // Ptr<Ipv4L3Protocol> ipv4 = StaticCast<Ipv4L3Protocol> ((sourceNodes.Get(sourceN))->GetObject<Ipv4> ()); // Get Ipv4 instance of the node
  // Ipv4Address addr = ipv4->GetAddress (1, 0).GetLocal();
  //NS_LOG_DEBUG("flow id: " << flow_id << " source node: " <<  sourceN << " sink node: " << sinkN << " start time: " << flow_start <<" deadline: " << deadline);
}

Time getDeadline(uint32_t flow_size)
{
  double dead = 0;
  if (flow_size < 1000000)
    dead = 0.001 + flow_size * 8.0/ (2.5 * 1e9);
  else
    dead = flow_size* 8.0 / (2.5*1e9);
  return Seconds(dead);
}

uint32_t getFlowSize(uint32_t type)
{
  uint32_t num = 0;
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
      if (x < 0.4)num = 0.1 + int(0.9 * xx);
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

double getAvgInterval()
{
  double avg_interarrival = 100; //ms
  if (num_hosts_per_leaf == 32)
    {
      if (traffic_type == WEBSEARCH)
        {
          switch (load)
            {
              case 30: avg_interarrival = 1.0/13; break;
              case 40: avg_interarrival = 1.0/18; break;
              case 50: avg_interarrival = 1.0/23; break;
              case 60: avg_interarrival = 1.0/27; break;
              case 70: avg_interarrival = 1.0/32; break;
              case 80: avg_interarrival = 1.0/36; break;
              default: std::cout << "undefined load" << std::endl; exit(2); 
            }
        }
      else if (traffic_type == DATAMINING)
        {
          switch (load)
            {
              case 30: avg_interarrival = 1.0/16; break;
              case 40: avg_interarrival = 1.0/21; break;
              case 50: avg_interarrival = 1.0/27; break;
              case 60: avg_interarrival = 1.0/32; break;
              case 70: avg_interarrival = 1.0/37; break;
              case 80: avg_interarrival = 1.0/42; break;  //?
              default: std::cout << "undefined load" << std::endl; exit(2); 
            }
        }
    }
  else if (num_hosts_per_leaf == 16)
    {
      if (traffic_type == WEBSEARCH)
        {
          switch (load)
            {
              case 30: avg_interarrival = 1.0/3.25; break;
              case 40: avg_interarrival = 1.0/4.5; break;
              case 50: avg_interarrival = 1.0/5.75; break;
              case 60: avg_interarrival = 1.0/6.76; break;
              case 70: avg_interarrival = 1.0/8; break;
              case 80: avg_interarrival = 1.0/9; break;
              default: std::cout << "undefined load" << std::endl; exit(2); 
            }
        }
      else if (traffic_type == DATAMINING)
        {
          switch (load)
            {
              case 30: avg_interarrival = 1.0/4; break;
              case 40: avg_interarrival = 1.0/5.25; break;
              case 50: avg_interarrival = 1.0/6.75; break;
              case 60: avg_interarrival = 1.0/8; break;
              case 70: avg_interarrival = 1.0/9.25; break;
              case 80: avg_interarrival = 1.0/10.5; break;  //?
              default: std::cout << "undefined load" << std::endl; exit(2); 
            }
        }
    }
  
  return avg_interarrival / 1000; //teturn seconds
}

void setUpTraffic()
{
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  Ptr<ExponentialRandomVariable> exp = CreateObject<ExponentialRandomVariable> ();
  // uint32_t mean_flow_size_web  =  0.3 * (1 + 9*0.5) + 0.3 * (10 + 90*0.5) + 0.15 * (100 + 900*0.5) + 0.24*(1000 + 9000*0.5) + 0.01*(40000*0.5);
  // uint32_t mean_flow_size_data  =  0.4 * (0.1 + 0.9*0.5) + 0.35 * (1 + 9*0.5) + 0.1 * (10 + 990*0.5) + 0.14*(1000 + 9000*0.5) + 0.01*(40000*0.5);
  // uint32_t mean_flow_size = traffic_type = WEBSEARCH ? mean_flow_size_web : mean_flow_size_data;
  // NS_LOG_UNCOND("Avg of flow bits.. "<<mean_flow_size);

  // double link_rate = DataRate(edge_datarate).GetBitRate();
  // double lambda = (link_rate * load * hosts.GetN() ) / (mean_flow_size * 8);
  // double avg_interarrival = 1/lambda;
  
  // exp->SetAttribute("Mean", DoubleValue(avg_interarrival));
  // std::cout<<"lambda is "<<lambda<< " avg_interarrival "<<avg_interarrival<<" meanflowsize "<<mean_flow_size<<" link_rate "<<link_rate<<" load "<<load<<std::endl;
  
  double avg_interval = getAvgInterval(); //s
  exp->SetAttribute("Mean", DoubleValue(avg_interval));

  double flow_start_time =global_start_time.GetSeconds();
  uint32_t flow_id = 0;
  
  while (flow_start_time < flow_stop_time.GetSeconds())
    {
      uint32_t source_node = uv->GetInteger(0, hosts.GetN() - 1);  //
      uint32_t rack = (source_node/num_hosts_per_leaf) + uv->GetInteger(1, num_leafs-1);  //??
      rack = rack%num_leafs;
      uint32_t sink_node = rack*num_hosts_per_leaf + uv->GetInteger(0, num_hosts_per_leaf - 1);
      uint32_t flow_size = getFlowSize(traffic_type);
      Time deadline = Time(0);
      if (flow_id%5 == 0)
        deadline = getDeadline(flow_size);
      startFlow(source_node, sink_node, Seconds(flow_start_time), flow_size, flow_id, deadline);
      flow_start_time = flow_start_time + exp->GetValue();
      NS_LOG_INFO ("flow id: " << flow_id << " src: " << source_node << " dst: " << sink_node << "flow_start_time:" << flow_start_time*1000 << "ms." << " flow size: " << flow_size << " deadline: " << deadline.GetSeconds() << "s");
      flow_id++; 
    }
}

void
SetupConfig (void)
{
  //configure log system
  // if (use_model == DCTCP_MODEL)
  //   LogComponentEnable ("DctcpSocket", LOG_LEVEL_ALL);
  // else if (use_model == D2TCP_MODEL)
  //   LogComponentEnable ("D2tcpSocket", LOG_LEVEL_ALL);
  // else if (use_model == DCMGR_MODEL)
  //   LogComponentEnable ("DcmgrSocket", LOG_LEVEL_INFO);

  //MySendApp config
  Config::SetDefault ("ns3::MySendApp::PacketSize", UintegerValue (1460));
  Config::SetDefault ("ns3::MySendApp::DataRate", DataRateValue (DataRate (edge_datarate)));

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

  // TCP params
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1460));
  Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (Time("20ms")));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue(2));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue(500000000));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue(tcp_rwndmax));//512k 524288; 1M 1048576, 128K 131072, 256K 262144
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  Config::SetDefault ("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue (initial_ssh));
  // Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));

  // enable ECN
  Config::SetDefault ("ns3::TcpSocketBase::UseEcn", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (true));

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

}


CommandLine addCmdOptions(void)
{
  std::string pathOut ("."); // Current directory

  global_start_time = Seconds (0);
  flow_stop_time = Seconds (1);

  num_spines = 8; 
  num_leafs = 8;
  num_hosts_per_leaf = 32;
  load = 80;
  seed = 1;
  traffic_type = 0;

  fabric_datarate = "20Gbps";
  fabric_delay = "20us";
  edge_datarate = "10Gbps";
  edge_delay = "10us";
  packet_size = 1500;  //bytes
  fabric_queue_size = 600; //packet, so as bellow
  fabric_threshold = 130;
  edge_queue_size = 300;
  edge_threshold = 65;
  rcos = 3;
  use_model = DCTCP_MODEL;
  m_g = 1.0 / 16;
  tcp_rwndmax =  262144; //512k 524288; 1M 1048576, 128K 131072, 256K 262144
  initial_ssh = 250000; //initial threshold;

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

  // RED params
  cmd.AddValue ("fabricThreshold", "the packet thread in the queue", fabric_threshold);
  cmd.AddValue ("fabricQueueSize", "the maximun packets in the queue", fabric_queue_size);
  cmd.AddValue ("edgeThreshold", "the packet thread in the queue", edge_threshold);
  cmd.AddValue ("edgeQueueSize", "the maximun packets in the queue", edge_queue_size);

  //dcmgr or dctcp or d2tcp
  cmd.AddValue ("m_g", "the weight of dcmgr of dctcp", m_g);

  cmd.AddValue ("pathOut", "Path to save results from --writeForPlot/--writePcap/--writeFlowMonitor", pathOut);
  cmd.AddValue ("rcos", "increase rate when rwnd < wmin", rcos);

  return cmd;
}

int main (int argc, char *argv[])
{
  std::cout << "flow id,fct,start time,stop time,flow size,deadline,src,dst" << std::endl;
  //LogComponentEnable ("DctcpSocket", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("DcmgrSocket", LOG_LEVEL_DEBUG);
  LogComponentEnable ("mgrSocket", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("Ipv4GlobalRouting", LOG_LEVEL_ERROR);
  //LogComponentEnable ("dcmgrTest", LOG_LEVEL_INFO);
  //LogComponentEnable ("RttEstimator", LOG_LEVEL_FUNCTION);
  // LogComponentEnable ("RedQueueDisc", LOG_LEVEL_INFO);
  //LogComponentEnable ("Queue", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("DropTailQueue", LOG_LEVEL_DEBUG);
  LogComponentEnable ("MySendApp", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("PfifoFastQueueDisc", LOG_LEVEL_LOGIC);
  //LogComponentEnable ("MySinkApp", LOG_LEVEL_INFO);
  //LogComponentEnable ("TcpSocketBase", LOG_LEVEL_FUNCTION);

  CommandLine cmd = addCmdOptions();
  cmd.Parse (argc, argv);

  SetupConfig ();

  //Random seeds
  RngSeedManager::SetSeed(10);
  RngSeedManager::SetRun(seed);

  global_stop_time = flow_stop_time + Seconds(2);
  
  createTopology();
  //SetupTopo (10, 1, link_data_rate, link_delay);
  
  setUpTraffic();


  std::cout << "simulation start" << std::endl;
  Simulator::Stop (global_stop_time);
  Simulator::Run ();



  Simulator::Destroy ();

  return 0;
}


/*void
SetupTopo (uint32_t srcCount, uint32_t dstCount, const DataRate& bandwidth, const Time& delay)
{
  NS_LOG_INFO ("Create nodes");
  srcs.Create (srcCount);
  routers.Create (2);
  dsts.Create (dstCount);

  NS_LOG_INFO ("Setup node name");
  SetupName ();

  NS_LOG_INFO ("Install internet stack on all nodes.");
  InternetStackHelper internet;
  internet.Install (srcs);
  internet.Install (routers);
  internet.Install (dsts);

  Ipv4AddressHelper ipv4AddrHelper;
  ipv4AddrHelper.SetBase ("10.1.1.0", "255.255.255.0");

  PointToPointHelper p2pHelper;
  p2pHelper.SetQueue ("ns3::DropTailQueue");
  p2pHelper.SetDeviceAttribute ("DataRate", DataRateValue (bandwidth));
  p2pHelper.SetChannelAttribute ("Delay", TimeValue (delay));

  TrafficControlHelper pfifoHelper;
  uint16_t handle = pfifoHelper.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "Limit", UintegerValue (1000));
  pfifoHelper.AddInternalQueues (handle, 3, "ns3::DropTailQueue", "MaxPackets", UintegerValue (1000));

  NS_LOG_INFO ("Setup src nodes");
  for (auto it = srcs.Begin (); it != srcs.End (); ++it)
    {
      NetDeviceContainer devs = p2pHelper.Install (NodeContainer (*it, routers.Get (0)));
      pfifoHelper.Install (devs);
      ipv4AddrHelper.Assign (devs);
      ipv4AddrHelper.NewNetwork ();
    }
  NS_LOG_INFO ("Setup dst nodes");
  for (auto it = dsts.Begin (); it != dsts.End (); ++it)
    {
      NetDeviceContainer devs = p2pHelper.Install (NodeContainer (routers.Get (1), *it));
      pfifoHelper.Install (devs);
      dst_interfaces.Add (ipv4AddrHelper.Assign (devs).Get (1));
      ipv4AddrHelper.NewNetwork ();
    }
  NS_LOG_INFO ("Setup router nodes");
  {
    NetDeviceContainer devs = p2pHelper.Install (routers);
    // only backbone link has RED queue disc
    TrafficControlHelper redHelper;
    redHelper.SetRootQueueDisc ("ns3::RedQueueDisc",
                                "LinkBandwidth", DataRateValue (bandwidth),
                                "LinkDelay", TimeValue (delay));
    redHelper.Install (devs);
    ipv4AddrHelper.Assign (devs);
  }
  // Set up the routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
}*/

/*void
SetupDsServer (NodeContainer nodes, uint16_t port)
{
  PacketSinkHelper serverHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer serverApps = serverHelper.Install (nodes);
  serverApps.Start (server_start_time);
  serverApps.Stop (server_stop_time);
  for (auto it = serverApps.Begin (); it != serverApps.End (); ++it)
    {
      (*it)->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTrace));
    }
}


void
SetupDsClient (NodeContainer nodes, const Address &serverAddr,
               const Time &startTime, uint64_t flowSize, const Time &deadline)
{
  static uint32_t index = 1;
  BulkSendHelper clientHelper ("ns3::TcpSocketFactory", serverAddr);
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (flowSize));
  ApplicationContainer clientApps = clientHelper.Install (nodes);
  clientApps.Start (startTime);
  clientApps.Stop (client_stop_time);
  for(auto it = clientApps.Begin (); it != clientApps.End (); ++it)
    {
      // trace tx
      (*it)->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, index++));
      // trace socket create
      if (use_model == D2TCP_MODEL | use_model == DCMGR_MODEL)
        {
          (*it)->TraceConnectWithoutContext ("SocketCreate", MakeBoundCallback (&SocketCreateTrace, flowSize, deadline));
        }
    }
}

void
SetupApp (bool enableLS, bool enableDS, bool enableCS)
{
  if (enableLS)
    {
      ///\todo ls setup
    }
  if (enableDS)
    {
      uint16_t port = 50000;
      uint64_t MB = 1024 * 1024;
      SetupDsServer (dsts.Get (0), port);
      InetSocketAddress serverAddr (dst_interfaces.GetAddress (0), port);
      SetupDsClient (srcs.Get (0), serverAddr, client_start_time, 150 * MB, Time ("3000ms"));
      SetupDsClient (srcs.Get (1), serverAddr, client_start_time, 220 * MB, Time ("8000ms"));
      SetupDsClient (srcs.Get (2), serverAddr, client_start_time, 350 * MB, Time ("50000ms"));
      SetupDsClient (srcs.Get (3), serverAddr, client_start_time, 500 * MB, Time ("60000ms"));
    }
  if (enableCS)
    {
      ///\todo cs setup
    }
}*/