/* DataCenter TCP (DCTCP) congestion control.
 *
 * http://simula.stanford.edu/~alizade/Site/DCTCP.html
 *
 * This is an implementation of DCTCP over Reno, an enhancement to the
 * TCP congestion control algorithm designed for data centers. DCTCP
 * leverages Explicit Congestion Notification (ECN) in the network to
 * provide multi-bit feedback to the end hosts. DCTCP's goal is to meet
 * the following three data center transport requirements:
 *
 *  - High burst tolerance (incast due to partition/aggregate)
 *  - Low latency (short flows, queries)
 *  - High throughput (continuous data updates, large file transfers)
 *    with commodity shallow buffered switches
 *
 * The algorithm is described in detail in the following two papers:
 *
 * 1) Mohammad Alizadeh, Albert Greenberg, David A. Maltz, Jitendra Padhye,
 *    Parveen Patel, Balaji Prabhakar, Sudipta Sengupta, and Murari Sridharan:
 *      "Data Center TCP (DCTCP)", Data Center Networks session
 *      Proc. ACM SIGCOMM, New Delhi, 2010.
 *   http://simula.stanford.edu/~alizade/Site/DCTCP_files/dctcp-final.pdf
 *
 * 2) Mohammad Alizadeh, Adel Javanmard, and Balaji Prabhakar:
 *      "Analysis of DCTCP: Stability, Convergence, and Fairness"
 *      Proc. ACM SIGMETRICS, San Jose, 2011.
 *   http://simula.stanford.edu/~alizade/Site/DCTCP_files/dctcp_analysis-full.pdf
 *
 * Initial prototype from Abdul Kabbani, Masato Yasuda and Mohammad Alizadeh.
 *
 * Authors:
 *
 *	Daniel Borkmann <dborkman@redhat.com>
 *	Florian Westphal <fw@strlen.de>
 *	Glenn Judd <glenn.judd@morganstanley.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <net/tcp.h>
#include <linux/inet_diag.h>
#include "table.h"

#define DCTCP_MAX_ALPHA	1024U

/********************added by Yunxian Wu*****************/

#define W_MAX   10000U
static unsigned int Wmax_BDP __read_mostly = 10000;
module_param(Wmax_BDP, uint, 0644);
MODULE_PARM_DESC(Wmax_BDP, "Wmax_BDP: max send window size= link bandwidth * link RTT");

/********************end*****************/

struct dctcp {
/********************added by Yunxian Wu*****************/
	int deadline;
	int size;
	struct timeval tv_start;
	u32 Wmax;
/*********************end**********************************/
	u32 acked_bytes_ecn;
	u32 acked_bytes_total;
	u32 prior_snd_una;
	u32 prior_rcv_nxt;
	u32 dctcp_alpha;
	u32 next_seq;
	u32 ce_state;
	u32 delayed_ack_reserved;
};

static unsigned int dctcp_shift_g __read_mostly = 4; /* g = 1/2^4 */
module_param(dctcp_shift_g, uint, 0644);
MODULE_PARM_DESC(dctcp_shift_g, "parameter g for updating dctcp_alpha");

static unsigned int dctcp_alpha_on_init __read_mostly = DCTCP_MAX_ALPHA;
module_param(dctcp_alpha_on_init, uint, 0644);
MODULE_PARM_DESC(dctcp_alpha_on_init, "parameter for initial alpha value");

static unsigned int dctcp_clamp_alpha_on_loss __read_mostly;
module_param(dctcp_clamp_alpha_on_loss, uint, 0644);
MODULE_PARM_DESC(dctcp_clamp_alpha_on_loss,
		 "parameter for clamping alpha on loss");

static struct tcp_congestion_ops dctcp_reno;

static void dctcp_reset(const struct tcp_sock *tp, struct dctcp *ca)
{
	ca->next_seq = tp->snd_nxt;

	ca->acked_bytes_ecn = 0;
	ca->acked_bytes_total = 0;
}

static void dctcp_init(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
/**********************************added by Yunxiang Wu********************************/

//	if ((tp->ecn_flags & TCP_ECN_OK) || (sk->sk_state == TCP_LISTEN ||  sk->sk_state == TCP_CLOSE)) 
/***************************************end**************************************/
	{
		struct dctcp *ca = inet_csk_ca(sk);

		ca->prior_snd_una = tp->snd_una;
		ca->prior_rcv_nxt = tp->rcv_nxt;

		ca->dctcp_alpha = min(dctcp_alpha_on_init, DCTCP_MAX_ALPHA);

		ca->delayed_ack_reserved = 0;
		ca->ce_state = 0;
/********************************added by Yunxiang Wu****************************/
		ca->Wmax=Wmax_BDP;
		//recording the start time
		do_gettimeofday(&(ca->tv_start));
/***********************************end*********************************************/

		dctcp_reset(tp, ca);
		return;
	}

	/* No ECN support? Fall back to Reno. Also need to clear
	 * ECT from sk since it is set during 3WHS for DCTCP.
	 */
	inet_csk(sk)->icsk_ca_ops = &dctcp_reno;
	INET_ECN_dontxmit(sk);
}

/*
 * calculate alpha ^ d
 * alpha is in [0, 1024], corresponding to [0, 1] in D2TCP.
 * d should be in [64, 256], corresponding to [0.5, 2] in D2TCP.
 *
 * The 1-D exp_results array stores the results, in which every consecutive
 * 193 elements, starting at the indices that are multiple of 193,
 * correspond to one value of alpha and 193 values of d (256 - 64 + 1).
 * So exp(alpha, d) = exp_results[193 * alpha + d - 64].
 */
static inline u32 d2tcp_exp(u32 alpha, u16 d)
{
	return exp_results[(alpha << 7) + (alpha << 6) + alpha + d - 64];
}


/******************added by Yunxiang Wu**********************/
/*
**get the time remaining until its deadline expires**
*/
u32 get_usec_remaining(const struct dctcp *ca)
{
	u32 usec, sec, deadline_us, time_us;
	struct timeval tv_now;
	do_gettimeofday(&tv_now);
	usec = tv_now.tv_usec - ca->tv_start.tv_usec;
	sec = tv_now.tv_sec - ca->tv_start.tv_sec;
	deadline_us = ca->deadline*1000;
	time_us = usec - sec*1000000;

	if(deadline_us > time_us)
		return (deadline_us - time_us);
	else
		return 0;
}

u32 d2tcp_d(const struct dctcp *ca, struct sock *sk)
{
	//u32 Tc;
	u32 B, D, d,tmp;
	u32 rtt_us, mss;
	struct tcp_sock *tp = tcp_sk(sk);


	if (tp->srtt_us) {		/* any RTT sample yet? */
		rtt_us = max(tp->srtt_us >> 3, 1U);
	} else {			 /* no RTT sample yet */
		rtt_us = USEC_PER_MSEC;	 /* use nominal default RTT */
	}

	mss = tp->mss_cache;

	//The number of bytes remaining to transmit	
	B = ca->size - ca->acked_bytes_total - (tp->snd_nxt - tp->snd_una);

	//printk("\n B=%d,ca->size=%d, ca->bytes_acked_total=%d,tp->snd_nxt - tp->snd_una=%d",B,ca->size, ca->acked_bytes_total, (tp->snd_nxt - tp->snd_una));

	//Tc = B/(0.75*W);
	// The time remaining until its deadline expires
	D = get_usec_remaining(ca);
	
if(D == 0)
{
	//printk("\nD<=0, what should i do?");
	return 128;
}
	//Tc = B*RTT/(0.75*W*mss);   d=Tc/D;	d=4*B*TRR/(3*W*D*mss);


/*	d=128*B*4*RTT;
	//1000000 for usec->sec; 
	tmp = 3*(tp->snd_cwnd)*D*mss;*/

	d=(128*B/mss)*(rtt_us*4);
	//1000000 for usec->sec; 
	tmp = 3*(tp->snd_cwnd)*D;

	do_div(d, tmp);

//printk("\nsk=%p,rtt_us=%d,mss=%d, d=TC/D,,,B=%d, snd_cwnd=%d, D=%d, d=%d, tmp=%d",sk, rtt_us, mss, B, tp->snd_cwnd,D,d, tmp);
	return d;
}

/*********************end ****************************************/
static u32 dctcp_ssthresh(struct sock *sk)
{
	const struct dctcp *ca = inet_csk_ca(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	u32 p;
	u16 d;

	d = 64;
/**************************added by Yunxiang Wu*********************/
	d=d2tcp_d(ca, sk);
/***************************end**************************************/

	d = min_t(u16, max_t(u16, d, 64), 256);	//ensure d in [64, 256] -> [0.5, 2]
	p = d2tcp_exp(ca->dctcp_alpha, d);	//p = alpha ^ d

/**************************added by Yunxiang Wu*********************/
//	printk("\nsk=%p,ssthresh=%d, d=%d",sk,max(tp->snd_cwnd - ((tp->snd_cwnd * p) >> 11U), 2U), d);
/***************************end**************************************/
	return max(tp->snd_cwnd - ((tp->snd_cwnd * p) >> 11U), 2U);
	//return max(tp->snd_cwnd - ((tp->snd_cwnd * ca->dctcp_alpha) >> 11U), 2U);
}

/* Minimal DCTP CE state machine:
 *
 * S:	0 <- last pkt was non-CE
 *	1 <- last pkt was CE
 */

static void dctcp_ce_state_0_to_1(struct sock *sk)
{
	struct dctcp *ca = inet_csk_ca(sk);
	struct tcp_sock *tp = tcp_sk(sk);

	/* State has changed from CE=0 to CE=1 and delayed
	 * ACK has not sent yet.
	 */
	if (!ca->ce_state && ca->delayed_ack_reserved) {
		u32 tmp_rcv_nxt;

		/* Save current rcv_nxt. */
		tmp_rcv_nxt = tp->rcv_nxt;

		/* Generate previous ack with CE=0. */
		tp->ecn_flags &= ~TCP_ECN_DEMAND_CWR;
		tp->rcv_nxt = ca->prior_rcv_nxt;

		tcp_send_ack(sk);

		/* Recover current rcv_nxt. */
		tp->rcv_nxt = tmp_rcv_nxt;
	}

	ca->prior_rcv_nxt = tp->rcv_nxt;
	ca->ce_state = 1;

	tp->ecn_flags |= TCP_ECN_DEMAND_CWR;
}

static void dctcp_ce_state_1_to_0(struct sock *sk)
{
	struct dctcp *ca = inet_csk_ca(sk);
	struct tcp_sock *tp = tcp_sk(sk);

	/* State has changed from CE=1 to CE=0 and delayed
	 * ACK has not sent yet.
	 */
	if (ca->ce_state && ca->delayed_ack_reserved) {
		u32 tmp_rcv_nxt;

		/* Save current rcv_nxt. */
		tmp_rcv_nxt = tp->rcv_nxt;

		/* Generate previous ack with CE=1. */
		tp->ecn_flags |= TCP_ECN_DEMAND_CWR;
		tp->rcv_nxt = ca->prior_rcv_nxt;

		tcp_send_ack(sk);

		/* Recover current rcv_nxt. */
		tp->rcv_nxt = tmp_rcv_nxt;
	}

	ca->prior_rcv_nxt = tp->rcv_nxt;
	ca->ce_state = 0;

	tp->ecn_flags &= ~TCP_ECN_DEMAND_CWR;
}

static void dctcp_update_alpha(struct sock *sk, u32 flags)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct dctcp *ca = inet_csk_ca(sk);
	u32 acked_bytes = tp->snd_una - ca->prior_snd_una;

//printk("\ninto a ack:sk=%p, tp->bytes_acked=%d,acked_bytes_total=%d, tp->snd_una=%d, ca->prior_snd_una=%d\n",sk,tp->bytes_acked,ca->acked_bytes_total,tp->snd_una , ca->prior_snd_una);
	/* If ack did not advance snd_una, count dupack as MSS size.
	 * If ack did update window, do not count it at all.
	 */
	if (acked_bytes == 0 && !(flags & CA_ACK_WIN_UPDATE))
		acked_bytes = inet_csk(sk)->icsk_ack.rcv_mss;
	if (acked_bytes) {
		ca->acked_bytes_total += acked_bytes;
		ca->prior_snd_una = tp->snd_una;

		if (flags & CA_ACK_ECE)
			ca->acked_bytes_ecn += acked_bytes;
	}

	/* Expired RTT */
	if (!before(tp->snd_una, ca->next_seq)) {
		/* For avoiding denominator == 1. */
		if (ca->acked_bytes_total == 0)
			ca->acked_bytes_total = 1;

		/* alpha = (1 - g) * alpha + g * F */
		ca->dctcp_alpha = ca->dctcp_alpha -
				  (ca->dctcp_alpha >> dctcp_shift_g) +
				  (ca->acked_bytes_ecn << (10U - dctcp_shift_g)) /
				  ca->acked_bytes_total;

		if (ca->dctcp_alpha > DCTCP_MAX_ALPHA)
			/* Clamp dctcp_alpha to max. */
			ca->dctcp_alpha = DCTCP_MAX_ALPHA;

		dctcp_reset(tp, ca);
	}
}

static void dctcp_state(struct sock *sk, u8 new_state)
{
	if (dctcp_clamp_alpha_on_loss && new_state == TCP_CA_Loss) {
		struct dctcp *ca = inet_csk_ca(sk);

		/* If this extension is enabled, we clamp dctcp_alpha to
		 * max on packet loss; the motivation is that dctcp_alpha
		 * is an indicator to the extend of congestion and packet
		 * loss is an indicator of extreme congestion; setting
		 * this in practice turned out to be beneficial, and
		 * effectively assumes total congestion which reduces the
		 * window by half.
		 */
		ca->dctcp_alpha = DCTCP_MAX_ALPHA;
	}
}

static void dctcp_update_ack_reserved(struct sock *sk, enum tcp_ca_event ev)
{
	struct dctcp *ca = inet_csk_ca(sk);

	switch (ev) {
	case CA_EVENT_DELAYED_ACK:
		if (!ca->delayed_ack_reserved)
			ca->delayed_ack_reserved = 1;
		break;
	case CA_EVENT_NON_DELAYED_ACK:
		if (ca->delayed_ack_reserved)
			ca->delayed_ack_reserved = 0;
		break;
	default:
		/* Don't care for the rest. */
		break;
	}
}

static void dctcp_cwnd_event(struct sock *sk, enum tcp_ca_event ev)
{
	switch (ev) {
	case CA_EVENT_ECN_IS_CE:
		dctcp_ce_state_0_to_1(sk);
		break;
	case CA_EVENT_ECN_NO_CE:
		dctcp_ce_state_1_to_0(sk);
		break;
	case CA_EVENT_DELAYED_ACK:
	case CA_EVENT_NON_DELAYED_ACK:
		dctcp_update_ack_reserved(sk, ev);
		break;
	default:
		/* Don't care for the rest. */
		break;
	}
}
/*
static size_t dctcp_get_info(struct sock *sk, u32 ext, struct sk_buff *skb)
{
	const struct dctcp *ca = inet_csk_ca(sk);

	if (ext & (1 << (INET_DIAG_DCTCPINFO - 1)) ||
	    ext & (1 << (INET_DIAG_VEGASINFO - 1))) {
		struct tcp_dctcp_info info;

		memset(&info, 0, sizeof(info));
		if (inet_csk(sk)->icsk_ca_ops != &dctcp_reno) {
			info.dctcp_enabled = 1;
			info.dctcp_ce_state = (u16) ca->ce_state;
			info.dctcp_alpha = ca->dctcp_alpha;
			info.dctcp_ab_ecn = ca->acked_bytes_ecn;
			info.dctcp_ab_tot = ca->acked_bytes_total;
		}

		nla_put(skb, INET_DIAG_DCTCPINFO, sizeof(info), &info);
	}
}*/

static size_t dctcp_get_info(struct sock *sk, u32 ext, int *attr,
			     union tcp_cc_info *info)
{
	const struct dctcp *ca = inet_csk_ca(sk);

	/* Fill it also in case of VEGASINFO due to req struct limits.
	 * We can still correctly retrieve it later.
	 */
	if (ext & (1 << (INET_DIAG_DCTCPINFO - 1)) ||
	    ext & (1 << (INET_DIAG_VEGASINFO - 1))) {
		memset(&info->dctcp, 0, sizeof(info->dctcp));
		if (inet_csk(sk)->icsk_ca_ops != &dctcp_reno) {
			info->dctcp.dctcp_enabled = 1;
			info->dctcp.dctcp_ce_state = (u16) ca->ce_state;
			info->dctcp.dctcp_alpha = ca->dctcp_alpha;
			info->dctcp.dctcp_ab_ecn = ca->acked_bytes_ecn;
			info->dctcp.dctcp_ab_tot = ca->acked_bytes_total;
		}

		*attr = INET_DIAG_DCTCPINFO;
		return sizeof(info->dctcp);
	}
	return 0;
}


/**************************added by Yunxiang Wu*********************/
void d2tcp_tcp_cong_avoid_ai(struct tcp_sock *tp, u32 w, u32 acked)
{
	/* If credits accumulated at a higher w, apply them gently now. */
	if (tp->snd_cwnd_cnt >= w) {
		tp->snd_cwnd_cnt = 0;
		tp->snd_cwnd++;
	}

	tp->snd_cwnd_cnt += acked;
	if (tp->snd_cwnd_cnt >= w) {
		u32 delta = tp->snd_cwnd_cnt / w;

		tp->snd_cwnd_cnt -= delta * w;
		tp->snd_cwnd += delta;
	}
	tp->snd_cwnd = min(tp->snd_cwnd, tp->snd_cwnd_clamp);
}

u32 d2tcp_tcp_slow_start(struct tcp_sock *tp, u32 acked)
{
	u32 cwnd = min(tp->snd_cwnd + acked, tp->snd_ssthresh);

	acked -= cwnd - tp->snd_cwnd;
	tp->snd_cwnd = min(cwnd, tp->snd_cwnd_clamp);

	struct dctcp *ca = (struct dctcp *) tp->inet_conn.icsk_ca_priv;

	if(ca->deadline==0)
		tp->snd_cwnd = min(tp->snd_cwnd,ca->Wmax);

	return acked;
}

void d2tcp_tcp_reno_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (!tcp_is_cwnd_limited(sk))
		return;

	/* In "safe" area, increase. */
	if (tcp_in_slow_start(tp)) {
		acked = d2tcp_tcp_slow_start(tp, acked);
		if (!acked)
			return;
	}
	/* In dangerous area, increase slowly. */
	d2tcp_tcp_cong_avoid_ai(tp, tp->snd_cwnd, acked);
}

/***************************end**************************************/
static struct tcp_congestion_ops dctcp __read_mostly = {
	.init		= dctcp_init,
	.in_ack_event   = dctcp_update_alpha,
	.cwnd_event	= dctcp_cwnd_event,
	.ssthresh	= dctcp_ssthresh,
	.cong_avoid	= d2tcp_tcp_reno_cong_avoid,
	.undo_cwnd	= tcp_reno_undo_cwnd,
	.set_state	= dctcp_state,
	.get_info	= dctcp_get_info,
	.flags		= TCP_CONG_NEEDS_ECN,
	.owner		= THIS_MODULE,
	.name		= "d2tcp",
};

static struct tcp_congestion_ops dctcp_reno __read_mostly = {
	.ssthresh	= tcp_reno_ssthresh,
	.cong_avoid	= tcp_reno_cong_avoid,
	.undo_cwnd	= tcp_reno_undo_cwnd,
	.get_info	= dctcp_get_info,
	.owner		= THIS_MODULE,
	.name		= "dc2tcp_reno",
};

static int __init dctcp_register(void)
{
	BUILD_BUG_ON(sizeof(struct dctcp) > ICSK_CA_PRIV_SIZE);
	return tcp_register_congestion_control(&dctcp);
}

static void __exit dctcp_unregister(void)
{
	tcp_unregister_congestion_control(&dctcp);
}

module_init(dctcp_register);
module_exit(dctcp_unregister);

MODULE_AUTHOR("Daniel Borkmann <dborkman@redhat.com>");
MODULE_AUTHOR("Florian Westphal <fw@strlen.de>");
MODULE_AUTHOR("Glenn Judd <glenn.judd@morganstanley.com>");

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("D2TCP (incomplete)");
