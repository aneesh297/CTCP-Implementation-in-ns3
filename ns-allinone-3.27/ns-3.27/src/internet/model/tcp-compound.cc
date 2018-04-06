/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 ResiliNets, ITTC, University of Kansas
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
 * Author: Truc Anh N. Nguyen <annguyen@ittc.ku.edu>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 */

#include "tcp-Compound.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/log.h"

 #include "math.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpCompound");
NS_OBJECT_ENSURE_REGISTERED (TcpCompound);

TypeId
TcpCompound::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpCompound")
    .SetParent<TcpNewReno> ()
    .AddConstructor<TcpCompound> ()
    .SetGroupName ("Internet")

    //gg
    .AddAttribute ("Alpha", "Alpha for multiplicative increase for m_cWnd",
                   DoubleValue (0.125),
                   MakeDoubleAccessor (&TcpCompound::m_beta),
                   MakeDoubleChecker <double> (0.0))
    .AddAttribute ("Beta", "Beta for multiplicative decrease for m_cWnd",
                   DoubleValue (0.5),
                   MakeDoubleAccessor (&TcpCompound::m_beta),
                   MakeDoubleChecker <double> (0.0))
    .AddAttribute ("eta", "Eta for additive decrease for m_dwnd",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&TcpCompound::m_beta),
                   MakeDoubleChecker <double> (0.0))
    .AddAttribute ("k", "Exponent for multiplicative increase for m_cWnd",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&TcpCompound::m_beta),
                   MakeDoubleChecker <double> (0.0))
    .AddAttribute ("Gamma", "Upper bound of packets in network",
                   UintegerValue (30),
                   MakeUintegerAccessor (&TcpCompound::m_gamma),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Cwnd", "Loss-based congestion window",
                   UintegerValue (2),
                   MakeUintegerAccessor (&TcpCompound::m_cwnd),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Dwnd", "Delay-based congestion window",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TcpCompound::m_dwnd),
                   MakeUintegerChecker<uint32_t> ())
    //gg
  ;
  return tid;
}

TcpCompound::TcpCompound (void)
  : TcpNewReno (),

    m_alpha (0.125),
    m_beta (0.5),
    m_eta (1.0),
    m_k (0.75),

    m_gamma (1),
    m_baseRtt (Time::Max ()),
    m_minRtt (Time::Max ()),
    m_cntRtt (0),
    m_doingCompoundNow (true),
    m_begSndNxt (0)
{
  NS_LOG_FUNCTION (this);
}

TcpCompound::TcpCompound (const TcpCompound& sock)
  : TcpNewReno (sock),

    m_alpha (sock.m_alpha),
    m_beta (sock.m_beta),
    m_eta (sock.m_eta),
    m_k (sock.m_k).

    m_alpha (sock.m_alpha),
    m_beta (sock.m_beta),
    m_gamma (sock.m_gamma),
    m_baseRtt (sock.m_baseRtt),
    m_minRtt (sock.m_minRtt),
    m_cntRtt (sock.m_cntRtt),
    m_doingCompoundNow (true),
    m_begSndNxt (0)
{
  NS_LOG_FUNCTION (this);
}

TcpCompound::~TcpCompound (void)
{
  NS_LOG_FUNCTION (this);
}

Ptr<TcpCongestionOps>
TcpCompound::Fork (void)
{
  return CopyObject<TcpCompound> (this);
}

void
TcpCompound::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                     const Time& rtt)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);

  if (rtt.IsZero ())
    {
      return;
    }

  m_minRtt = std::min (m_minRtt, rtt);
  NS_LOG_DEBUG ("Updated m_minRtt = " << m_minRtt);

  m_baseRtt = std::min (m_baseRtt, rtt);
  NS_LOG_DEBUG ("Updated m_baseRtt = " << m_baseRtt);

  // Update RTT counter
  m_cntRtt++;
  NS_LOG_DEBUG ("Updated m_cntRtt = " << m_cntRtt);
}

void
TcpCompound::EnableCompound (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);

  m_doingCompoundNow = true;
  m_begSndNxt = tcb->m_nextTxSequence;
  m_cntRtt = 0;
  m_minRtt = Time::Max ();
}

void
TcpCompound::DisableCompound ()
{
  NS_LOG_FUNCTION (this);

  m_doingCompoundNow = false;
}

void
TcpCompound::CongestionStateSet (Ptr<TcpSocketState> tcb,
                              const TcpSocketState::TcpCongState_t newState)
{
  NS_LOG_FUNCTION (this << tcb << newState);
  if (newState == TcpSocketState::CA_OPEN)
    {
      EnableCompound (tcb);
    }
  else
    {
      DisableCompound ();
    }
}

void
TcpCompound::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (!m_doingCompoundNow)
    {
      // If Compound is not on, we follow NewReno algorithm
      NS_LOG_LOGIC ("Compound is not turned on, we follow NewReno algorithm.");
      TcpNewReno::IncreaseWindow (tcb, segmentsAcked);
      return;
    }

  if (tcb->m_lastAckedSeq >= m_begSndNxt)
    { // A Compound cycle has finished, we do Compound cwnd adjustment every RTT.

      NS_LOG_LOGIC ("A Compound cycle has finished, we adjust cwnd once per RTT.");

      // Save the current right edge for next Compound cycle
      m_begSndNxt = tcb->m_nextTxSequence;

      /*
       * We perform Compound calculations only if we got enough RTT samples to
       * insure that at least 1 of those samples wasn't from a delayed ACK.
       */
      if (m_cntRtt <= 2)
        {  // We do not have enough RTT samples, so we should behave like Reno
          NS_LOG_LOGIC ("We do not have enough RTT samples to do Compound, so we behave like NewReno.");
          TcpNewReno::IncreaseWindow (tcb, segmentsAcked);
        }
      else
        {
          NS_LOG_LOGIC ("We have enough RTT samples to perform Compound calculations");
          /*
           * We have enough RTT samples to perform Compound algorithm.
           * Now we need to determine if cwnd should be increased or decreased
           * based on the calculated difference between the expected rate and actual sending
           * rate and the predefined thresholds (alpha, beta, and gamma).
           */
          uint32_t diff;
          uint32_t targetCwnd;
          uint32_t segCwnd = tcb->GetCwndInSegments ();

          /*
           * Calculate the cwnd we should have. baseRtt is the minimum RTT
           * per-connection, minRtt is the minimum RTT in this window
           *
           * little trick:
           * desidered throughput is currentCwnd * baseRtt
           * target cwnd is throughput / minRtt
           */
          double tmp = m_baseRtt.GetSeconds () / m_minRtt.GetSeconds ();
          targetCwnd = segCwnd * tmp;
          NS_LOG_DEBUG ("Calculated targetCwnd = " << targetCwnd);
          NS_ASSERT (segCwnd >= targetCwnd); // implies baseRtt <= minRtt

          /*
           * Calculate the difference between the expected cWnd and
           * the actual cWnd
           */
          diff = segCwnd - targetCwnd;
          NS_LOG_DEBUG ("Calculated diff = " << diff);


          if ((tcb->m_cWnd < tcb->m_ssThresh) && (diff < m_gamma)
            {     
              // Slow start mode
              NS_LOG_LOGIC ("We are in slow start and diff < m_gamma, so we "
                            "follow NewReno slow start");
              TcpNewReno::SlowStart (tcb, segmentsAcked);
            }

          else if (diff < m_gamma)
            {
              NS_LOG_LOGIC ("No congestion nor packet loss");



              tcb->m_cWnd += static_cast<uint32_t> (m_alpha*pow(tcb->m_cWnd, m_k));

              double adder = static_cast<double> (tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get ();
              adder = std::max (1.0, adder);
              m_cwnd += static_cast<uint32_t> (adder);

              NS_ASSERT (tcb->m_cWnd >= m_cwnd);
              m_dwnd = tcb->m_cWnd - m_cwnd;
             
  
            }
          else if (diff >= m_gamma)
            {
              NS_LOG_LOGIC ("Only Congestion, no packet loss");

              double adder = static_cast<double> (tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get ();
              adder = std::max (1.0, adder);
              m_cwnd += static_cast<uint32_t> (adder);

              m_dwnd = static_cast<uint32_t>(std::max(1.0,static_cast<double>(m_dwnd) - m_eta * diff));

              tcb->m_cWnd = m_cwnd + m_dwnd;
            }
         
          }

        }

      // Reset cntRtt & minRtt every RTT
      m_cntRtt = 0;
      m_minRtt = Time::Max ();
    }
  else if (tcb->m_cWnd < tcb->m_ssThresh)
    {
      TcpNewReno::SlowStart (tcb, segmentsAcked);
    }
}

std::string
TcpCompound::GetName () const
{
  return "TcpCompound";
}

uint32_t
TcpCompound::GetSsThresh (Ptr<const TcpSocketState> tcb,
                       uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);
  return std::max (std::min (tcb->m_ssThresh.Get (), tcb->m_cWnd.Get () - tcb->m_segmentSize), 2 * tcb->m_segmentSize);
}

} // namespace ns3
