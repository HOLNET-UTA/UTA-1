/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2014 University of Washington
 *               2015 Universita' degli Studi di Napoli Federico II
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:  Stefano Avallone <stavallo@unina.it>
 *           Tom Henderson <tomhend@u.washington.edu>
 */

#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/object-factory.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/socket.h"
#include "my-fifo-queue-disc.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MyFifoQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (MyFifoQueueDisc);

TypeId MyFifoQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MyFifoQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<MyFifoQueueDisc> ()
    .AddAttribute ("Limit",
                   "The maximum number of packets accepted by this queue disc.",
                   UintegerValue (500),
                   MakeUintegerAccessor (&MyFifoQueueDisc::m_limit),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}

MyFifoQueueDisc::MyFifoQueueDisc ()
{
  NS_LOG_FUNCTION (this);
  indexBase = 0;
}

MyFifoQueueDisc::~MyFifoQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}


bool
MyFifoQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  if (GetNPackets () > m_limit)
    {
      NS_LOG_DEBUG ("Queue disc limit exceeded -- dropping packet");
      Drop (item);
      return false;
    }

  uint32_t index = 0;
  SocketQueueIndexTag queueIndexTag;
  if (item->GetPacket ()->PeekPacketTag (queueIndexTag))
    {
      index = queueIndexTag.GetQueueIndex ();
    }
  while (index >= GetNInternalQueues())
    {
      ObjectFactory factory;
      factory.SetTypeId ("ns3::DropTailQueue");
      factory.Set ("Mode", EnumValue (Queue::QUEUE_MODE_PACKETS));
      factory.Set ("MaxPackets", UintegerValue (m_limit));
      AddInternalQueue (factory.Create<Queue> ());
    }

  bool retval = GetInternalQueue (index)->Enqueue (item);

  // If Queue::Enqueue fails, QueueDisc::Drop is called by the internal queue
  // because QueueDisc::AddInternalQueue sets the drop callback

  NS_LOG_DEBUG ("Number packets of queue" << index << ": " << GetInternalQueue (index)->GetNPackets ());

  return retval;
}

Ptr<QueueDiscItem>
MyFifoQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<QueueDiscItem> item;

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      uint32_t index = (i + indexBase) % (GetNInternalQueues ());
      if ((item = StaticCast<QueueDiscItem> (GetInternalQueue (index)->Dequeue ())) != 0)
        {
          NS_LOG_DEBUG ("Popped from queue " << index << ": " << item);
          NS_LOG_DEBUG ("Number packets of queue " << index << ": " << GetInternalQueue (index)->GetNPackets ());
          indexBase = (index + 1) % (GetNInternalQueues ());
          return item;
        }
    }
  
  NS_LOG_DEBUG ("Queue empty");
  return item;
}

Ptr<const QueueDiscItem>
MyFifoQueueDisc::DoPeek (void) const
{
  NS_LOG_FUNCTION (this);

  Ptr<const QueueDiscItem> item;

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      item = StaticCast<const QueueDiscItem> (GetInternalQueue (i)->Peek ());
      NS_LOG_LOGIC ("Peeked from queue " << i << ": " << item);
      NS_LOG_LOGIC ("Number packets of queue " << i << ": " << GetInternalQueue (i)->GetNPackets ());
      return item;
    }

  NS_LOG_LOGIC ("Queue empty");
  return item;
}

bool
MyFifoQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("MyFifoQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () != 0)
    {
      NS_LOG_ERROR ("MyFifoQueueDisc needs no packet filter");
      return false;
    }

  if (GetNInternalQueues () == 0)
    {
      // create 1 DropTail queues with m_limit packets
      ObjectFactory factory;
      factory.SetTypeId ("ns3::DropTailQueue");
      factory.Set ("Mode", EnumValue (Queue::QUEUE_MODE_PACKETS));
      factory.Set ("MaxPackets", UintegerValue (m_limit));
      AddInternalQueue (factory.Create<Queue> ());
    }

  // if (GetNInternalQueues () != 31)
  //   {
  //     NS_LOG_ERROR ("MyFifoQueueDisc needs 31 internal queues");
  //     return false;
  //   }

  for (uint32_t i = 0; i < GetNInternalQueues(); i++)
    {
      if (GetInternalQueue (i)-> GetMode () != Queue::QUEUE_MODE_PACKETS)
        {
          NS_LOG_ERROR ("MyFifoQueueDisc internal queues operating in packet mode");
          return false;
        }
    }

  for (uint32_t i = 0; i < GetNInternalQueues(); i++)
    {
      if (GetInternalQueue (i)->GetMaxPackets () < m_limit)
        {
          NS_LOG_ERROR ("The capacity of some internal queue(s) is less than the queue disc capacity");
          return false;
        }
    }

  return true;
}

void
MyFifoQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
}

} // namespace ns3
