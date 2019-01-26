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

#define DCTCP_MAX_ALPHA	1024U

/***********************add by yunxiang Wu*******************/

#define W_MAX	10000U
static unsigned int Wmax_BDP __read_mostly = 10000; 
module_param(Wmax_BDP, uint, 0644);
MODULE_PARM_DESC(Wmax_BDP, "Wmax_BDP: max send window size= link bandwidth * link RTT");

struct dctcp {
/********************added by Yunxian Wu*****************/
        int deadline;
        int size;
        struct timeval tv_start;
        unsigned int mrg_r;
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
	u32 loss_cwnd;
};
/*****************end**********************/
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
//	if ((tp->ecn_flags & TCP_ECN_OK) || (sk->sk_state == TCP_LISTEN || sk->sk_state == TCP_CLOSE)) 
	{
		struct dctcp *ca = inet_csk_ca(sk);

		ca->prior_snd_una = tp->snd_una;
		ca->prior_rcv_nxt = tp->rcv_nxt;

		ca->dctcp_alpha = min(dctcp_alpha_on_init, DCTCP_MAX_ALPHA);

		ca->delayed_ack_reserved = 0;
		ca->loss_cwnd = 0;
		ca->ce_state = 0;

	ca->Wmax=Wmax_BDP;
	do_gettimeofday(&(ca->tv_start));
	ca->mrg_r = /*mrg_r*/3;
//printk("\nmrg ecn flow start: deadline=%d flow size=%d ", ca->deadline, ca->size);
		dctcp_reset(tp, ca);
		return;
	}

	/* No ECN support? Fall back to Reno. Also need to clear
	 * ECT from sk since it is set during 3WHS for DCTCP.
	 */
	inet_csk(sk)->icsk_ca_ops = &dctcp_reno;
	INET_ECN_dontxmit(sk);
}

static u32 dctcp_ssthresh(struct sock *sk)
{
	struct dctcp *ca = inet_csk_ca(sk);
	struct tcp_sock *tp = tcp_sk(sk);

	ca->loss_cwnd = tp->snd_cwnd;
	return max(tp->snd_cwnd - ((tp->snd_cwnd * ca->dctcp_alpha) >> 11U), 2U);
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
		u64 bytes_ecn = ca->acked_bytes_ecn;
		u32 alpha = ca->dctcp_alpha;

		/* alpha = (1 - g) * alpha + g * F */

		alpha -= min_not_zero(alpha, alpha >> dctcp_shift_g);
		if (bytes_ecn) {
			/* If dctcp_shift_g == 1, a 32bit value would overflow
			 * after 8 Mbytes.
			 */
			bytes_ecn <<= (10 - dctcp_shift_g);
			do_div(bytes_ecn, max(1U, ca->acked_bytes_total));

			alpha = min(alpha + (u32)bytes_ecn, DCTCP_MAX_ALPHA);
		}
		/* dctcp_alpha can be read from dctcp_get_info() without
		 * synchro, so we ask compiler to not use dctcp_alpha
		 * as a temporary variable in prior operations.
		 */
		WRITE_ONCE(ca->dctcp_alpha, alpha);
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

static u32 dctcp_cwnd_undo(struct sock *sk)
{
	const struct dctcp *ca = inet_csk_ca(sk);

	return max(tcp_sk(sk)->snd_cwnd, ca->loss_cwnd);
}

static struct tcp_congestion_ops dctcp __read_mostly = {
	.init		= dctcp_init,
	.in_ack_event   = dctcp_update_alpha,
	.cwnd_event	= dctcp_cwnd_event,
	.ssthresh	= dctcp_ssthresh,
	.cong_avoid	= tcp_reno_cong_avoid,
	.undo_cwnd	= dctcp_cwnd_undo,
	.set_state	= dctcp_state,
	.get_info	= dctcp_get_info,
	.flags		= TCP_CONG_NEEDS_ECN,
	.owner		= THIS_MODULE,
	.name		= "dctcp",
};

static struct tcp_congestion_ops dctcp_reno __read_mostly = {
	.ssthresh	= tcp_reno_ssthresh,
	.cong_avoid	= tcp_reno_cong_avoid,
	.undo_cwnd	= tcp_reno_undo_cwnd,
	.get_info	= dctcp_get_info,
	.owner		= THIS_MODULE,
	.name		= "dctcp-reno",
};

/**************************added by Yunxiang Wu********************/


static unsigned int mrg_ecn __read_mostly = 1; 
module_param(mrg_ecn, uint, 0644);
MODULE_PARM_DESC(mrg_ecn, "ECN enable or disable: 1 enable and 0 disable");

static unsigned int mrg_r __read_mostly = 3; 
module_param(mrg_r, uint, 0644);
MODULE_PARM_DESC(mrg_r, "parameter r for updating mrg_r_cos");

static struct tcp_congestion_ops mrg_tcp;


static void mrgtcp_reset(const struct tcp_sock *tp, struct dctcp *ca)
{
	ca->next_seq = tp->snd_nxt;

	ca->acked_bytes_ecn = 0;
	ca->acked_bytes_total = 0;
}

static void mrg_init(struct sock *sk)
{
        const struct tcp_sock *tp = tcp_sk(sk);
        struct dctcp *ca = inet_csk_ca(sk);

        ca->prior_snd_una = tp->snd_una;
        ca->prior_rcv_nxt = tp->rcv_nxt;

        ca->dctcp_alpha = min(dctcp_alpha_on_init, DCTCP_MAX_ALPHA);

        ca->delayed_ack_reserved = 0;
        ca->loss_cwnd = 0;
        ca->ce_state = 0;

        dctcp_reset(tp, ca);

	do_gettimeofday(&(ca->tv_start));
	ca->mrg_r = mrg_r;

	ca->Wmax=Wmax_BDP;

//printk("\ndeadline=%d,flow size=%d", ca->deadline, ca->size);
	if(mrg_ecn)//support ECN
        	return;

        /* No ECN support? Fall back to Reno. Also need to clear
         * ECT from sk since it is set during 3WHS for DCTCP.
         */
        inet_csk(sk)->icsk_ca_ops = &mrg_tcp;

        INET_ECN_dontxmit(sk);
}

/*
**get the time remaining until its deadline expires**
*/
u32 get_usec_remaining(const struct dctcp *ca)
{
        long usec, sec, deadline_us, time_us;
        struct timeval tv_now;
if((ca->deadline == 0))
	return 0;

        do_gettimeofday(&tv_now);
        usec = tv_now.tv_usec - ca->tv_start.tv_usec;
        sec = tv_now.tv_sec - ca->tv_start.tv_sec;
        deadline_us = ca->deadline*1000;
        time_us = usec - sec*1000000;
//printk("deadline_us=%ld,time_us=%ld,start.sec=%ld,start.usec=%ld---now.sec=%ld,now.usec=%ld",deadline_us,time_us,ca->tv_start.tv_sec,ca->tv_start.tv_usec,tv_now.tv_sec,tv_now.tv_usec);
        if(deadline_us > time_us)
                return (deadline_us - time_us);
        else
                return 0;
}

u32 mrg_W_min(const struct dctcp *ca, struct tcp_sock *tp)
{
	u32 rtt_us, Wmin, Sf, MSS;
	long Td;

	Td = get_usec_remaining(ca); 

	if( Td == 0)
	{
		return 0;
	}	


	if (tp->srtt_us) {              /* any RTT sample yet? */
                rtt_us = max(tp->srtt_us >> 3, 1U);
        } else {                         /* no RTT sample yet */
                rtt_us = USEC_PER_MSEC;  /* use nominal default RTT */
        }

	//ca->Wmax=(rtt_us*bandwidth_BDP_Mb)>>3;//bit to Byte

	MSS = tp->mss_cache;

	//The number of bytes remaining to transmit
        Sf = ca->size - ca->acked_bytes_total - (tp->snd_nxt - tp->snd_una);
if((Td*MSS) == 0)
{
	printk("\nSf=%lu, rtt_us=%lu, Td=%ld, MSS=%lu", Sf, rtt_us, Td, MSS);
return 0;
}

	//Wmin = (Sf * rtt_us)/(Td*MSS);
	Wmin = ((Sf/MSS) + 1)/((Td/rtt_us)+1);
	Wmin = max(2, Wmin);
	//printk("\nSf=%lu, rtt_us=%lu, Td=%ld, MSS=%lu", Sf, rtt_us, Td, MSS);
	return Wmin; 
}

u32 mrg_tcp_slow_start(struct tcp_sock *tp, u32 acked)
{
	//the acked seq numbers may consist of two part: one part for slow start and another part for congestion avoidence

	u32 cwnd, Wmin, cwnd2;
	struct dctcp *ca = (struct dctcp *) tp->inet_conn.icsk_ca_priv;

	Wmin = mrg_W_min(ca, tp);
//printk("\nslow start begin: Wmin=%d, snd_wnd=%d acked=%d snd_ssthresh=%d",Wmin,tp->snd_cwnd,acked,tp->snd_ssthresh);
	//tmp =0;//2018-9-2
	cwnd2 = min(tp->snd_cwnd + acked, tp->snd_ssthresh);

	if(Wmin > tp->snd_cwnd)// mrg slow start
	{
	//the last seq number in slow start part
		struct dctcp *ca = (struct dctcp *) tp->inet_conn.icsk_ca_priv;
		cwnd = min(tp->snd_cwnd + acked*(2*/*ca->mrg_r*/3 - 1), tp->snd_ssthresh);
		cwnd = min(cwnd, Wmin);
//printk("\n implement :mrg slow start");
	}
	else// the same as TCP reno
	{
		cwnd = min(tp->snd_cwnd + acked, tp->snd_ssthresh);
		if(ca->deadline==0)
			cwnd = min(cwnd, ca->Wmax);
	}
	//the number of congestion avoidence part
	acked -= cwnd2 - tp->snd_cwnd;
	//updating the CWS value for slow start part
	tp->snd_cwnd = min(cwnd, tp->snd_cwnd_clamp);


//printk("\nslow start end: Wmin=%lu, snd_cwnd=%lu snd_cwnd_clamp=%lu ",Wmin,tp->snd_cwnd, tp->snd_cwnd_clamp);
	//return the number of congestion avoidence part
	return acked;
}

/* In theory this is tp->snd_cwnd += 1 / tp->snd_cwnd (or alternative w),
 * for every packet that was ACKed.
 */
void mrg_tcp_cong_avoid_ai(struct tcp_sock *tp, u32 w, u32 acked)
{
	u32  Wmin;
	struct dctcp *ca = (struct dctcp *) tp->inet_conn.icsk_ca_priv;

	Wmin = mrg_W_min(ca, tp);
//printk("\nmrg_tcp_cong_avoid_ai end: Wmin=%lu, snd_wnd=%lu acked=%lu w=%lu snd_cwnd_cnt=%lu",Wmin,tp->snd_cwnd,acked,w,tp->snd_cwnd_cnt);

	/* If credits accumulated at a higher w, apply them gently now. */
	if (tp->snd_cwnd_cnt >= w) {
		tp->snd_cwnd_cnt = 0;
		/////////////////////////////////////
                if(Wmin > tp->snd_cwnd)
                {
                        tp->snd_cwnd = tp->snd_cwnd * (/*ca->mrg_r*/3) +1;
                        tp->snd_cwnd = min(Wmin, tp->snd_cwnd);
                }
///////////////////////////////////////
		else
		{
			tp->snd_cwnd++;
		}
	}

	tp->snd_cwnd_cnt += acked;
	if (tp->snd_cwnd_cnt >= w && (w > 0)) {
		u32 delta = tp->snd_cwnd_cnt / w;
////////////////////////////////////////
		if(Wmin > tp->snd_cwnd)
		{
			tp->snd_cwnd = ( tp->snd_cwnd * ( /*ca->mrg_r*/3 ) + 1 )*delta;
			tp->snd_cwnd = min(Wmin + delta - 1, tp->snd_cwnd);
		}
///////////////////////////////////////
		else
		{	
			tp->snd_cwnd += delta;
		}
		tp->snd_cwnd_cnt -= delta * w;
	}

	tp->snd_cwnd = min(tp->snd_cwnd, tp->snd_cwnd_clamp);
	//printk("\n implement :reno  cong avoid snd_cwnd=%d",tp->snd_cwnd);
//printk("\nmrg_tcp_cong_avoid_ai end: Wmin=%lu, snd_wnd=%lu",Wmin,tp->snd_cwnd);
}

void mrg_tcp_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
//printk("\nmrg_tcp_cong_avoid");
	struct tcp_sock *tp = tcp_sk(sk);

	if (!tcp_is_cwnd_limited(sk))
		return;

	/* In "safe" area, increase. */
	if (tcp_in_slow_start(tp)) {
		//acked = tcp_slow_start(tp, acked);//2018-9-2
		acked = mrg_tcp_slow_start(tp, acked);
		if (!acked)
			return;
	}
	/* In dangerous area, increase slowly. */
	mrg_tcp_cong_avoid_ai(tp, tp->snd_cwnd, acked);
	//tcp_cong_avoid_ai(tp, tp->snd_cwnd, acked);//2018-9-2
}




static struct tcp_congestion_ops mrg_dctcp __read_mostly = {
	.init		= dctcp_init,
	.in_ack_event   = dctcp_update_alpha,//each receiving ack
	.cwnd_event	= dctcp_cwnd_event,
	.ssthresh	= dctcp_ssthresh,////return the slow start threshold. If there is any ECN marked ACK during one RTT, this function happens at the time of receiving the last ACK. At this time, the slow start threshold equals  the sending congestion window
	.cong_avoid	= mrg_tcp_cong_avoid,
	.undo_cwnd	= dctcp_cwnd_undo,
	.set_state	= dctcp_state,
	.get_info	= dctcp_get_info,
	.flags		= TCP_CONG_NEEDS_ECN,
	.owner		= THIS_MODULE,
	.name		= "mrg_ecn",
};


static struct tcp_congestion_ops mrg_tcp __read_mostly = {
	.init		= mrg_init,
        .ssthresh       = tcp_reno_ssthresh,//return the slow start threshold. If there is any ECN marked ACK during one RTT, this function happens at the time of receiving the last ACK. At this time, the slow start threshold equals  the sending congestion window 
        .cong_avoid     = mrg_tcp_cong_avoid,//tcp_reno_cong_avoid,// caculate new sending congestion window. This  happens at the time of receiving each ACK
        .undo_cwnd      = tcp_reno_undo_cwnd,// the new value of cwnd which happens after loss
        .get_info       = dctcp_get_info,// need to be modified
        .owner          = THIS_MODULE,
        .name           = "mrg_tcp",
};
/****************************end***********************************/

static int __init dctcp_register(void)
{
	BUILD_BUG_ON(sizeof(struct dctcp) > ICSK_CA_PRIV_SIZE);
/*****************************added by Yunxiang Wu*******************/
	//return tcp_register_congestion_control(&dctcp);
	return tcp_register_congestion_control(&mrg_dctcp);
/***************************end********************************/
}

static void __exit dctcp_unregister(void)
{
	tcp_unregister_congestion_control(&mrg_dctcp);
}

module_init(dctcp_register);
module_exit(dctcp_unregister);

MODULE_AUTHOR("Daniel Borkmann <dborkman@redhat.com>");
MODULE_AUTHOR("Florian Westphal <fw@strlen.de>");
MODULE_AUTHOR("Glenn Judd <glenn.judd@morganstanley.com>");

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DataCenter TCP (DCTCP)");
