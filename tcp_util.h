#ifndef __TCP_UTIL_H__
#define __TCP_UTIL_H__

#include <math.h>
#include <stdint.h>

#include <rte_ip.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_tcp.h>
#include <rte_flow.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_ether.h>
#include <rte_atomic.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_mempool.h>

// TCP State enum
typedef enum {
	TCP_INIT,
    	TCP_LISTEN,
    	TCP_SYN_SENT,
    	TCP_SYN_RECV,
    	TCP_ESTABLISHED,
    	TCP_FIN_WAIT_I,
    	TCP_FIN_WAIT_II,
    	TCP_LAST_ACK,
    	TCP_CLOSING,
    	TCP_TIME_WAIT,
    	TCP_CLOSE_WAIT,
    	TCP_CLOSED,
} tcb_state_t;

// TCP Control Block
typedef struct tcp_control_block_s {
	// used only by the TX
	uint32_t		        		tcb_next_seq;
	uint32_t 						src_addr;
	uint32_t 						dst_addr;
	uint16_t						src_port;
	uint16_t						dst_port;
	uint64_t						instructions;
	double							randomness;

	// used only by the RX
	uint32_t						last_ack_recv;
	uint32_t 						last_seq_recv;

	// used by both RX/TX 
	rte_atomic32_t 					tcb_next_ack;
	rte_atomic16_t 					tcb_state;
	rte_atomic16_t 					tcb_rwin;
	
	// used only in the beginning
	uint32_t						tcb_seq_ini;
	uint32_t						tcb_ack_ini;
	struct rte_flow_item_eth		flow_eth;
	struct rte_flow_item_eth		flow_eth_mask;
	struct rte_flow_item_ipv4		flow_ipv4;
	struct rte_flow_item_ipv4		flow_ipv4_mask;
	struct rte_flow_item_tcp		flow_tcp;
	struct rte_flow_item_tcp		flow_tcp_mask;
	struct rte_flow_action_mark 	flow_mark_action;
	struct rte_flow_action_queue 	flow_queue_action;

} __rte_cache_aligned tcp_control_block_t;

typedef struct tcp_options_ws_s {
	uint8_t 	kind;
	uint8_t 	length;
	uint8_t 	shift;
	uint8_t 	nop;
} __attribute__((packed)) tcp_options_ws_t;

typedef struct tcp_options_mss_s {
	uint8_t 	kind;
	uint8_t 	length;
	uint16_t 	value;
} __attribute__((packed)) tcp_options_mss_t;

#define ETH_IPV4_TYPE_NETWORK		0x0008
#define HANDSHAKE_TIMEOUT_IN_US		500000
#define HANDSHAKE_RETRANSMISSION	4
#define SEQ_LEQ(a,b)		        ((int32_t)((a)-(b)) <= 0)
#define SEQ_LT(a,b)		        	((int32_t)((a)-(b)) < 0)

extern uint16_t dst_tcp_port;
extern uint32_t dst_ipv4_addr;
extern uint32_t src_ipv4_addr;
extern struct rte_ether_addr dst_eth_addr;
extern struct rte_ether_addr src_eth_addr;

extern uint64_t srv_distribution;
extern uint64_t srv_instructions;

extern uint64_t nr_flows;
extern uint32_t frame_size;
extern uint32_t tcp_payload_size;
extern struct rte_mempool *pktmbuf_pool_rx;
extern struct rte_mempool *pktmbuf_pool_tx;
extern tcp_control_block_t *tcp_control_blocks;

void init_tcp_blocks();
struct rte_mbuf* create_syn_packet(uint16_t i);
struct rte_mbuf *create_ack_packet(uint16_t i);
void fill_tcp_payload(uint8_t *payload, uint32_t length);
struct rte_mbuf* process_syn_ack_packet(struct rte_mbuf* pkt);
void fill_tcp_packet(tcp_control_block_t *block, struct rte_mbuf *pkt);
void hot_fill_tcp_packet(tcp_control_block_t *block, struct rte_mbuf *pkt);

#endif // __TCP_UTIL_H__
