#ifndef _BKDRFT_H
#define _BKDRFT_H

#include <rte_mbuf.h>

#define BKDRFT_CTRL_QUEUE (0)

/* Control Packet */
struct ctrl_pkt {
	uint8_t q;
};

/*
 * Send a packet, aware of control queue mechanism 
 * */
int send_pkt(int port, uint8_t qid,
	struct rte_mbuf **tx_pkts, uint16_t nb_pkts, bool send_ctrl_pkt,
	struct rte_mempool *tx_mbuf_pool);

#endif
