#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "util.h"
#include "tcp_util.h"
#include "dpdk_util.h"

#define PKT_RX_RSS_HASH      (1ULL << 1)
#define PKT_RX_FDIR          (1ULL << 2)

// Application parameters
uint64_t rate;
uint32_t seed;
uint64_t duration;
uint64_t nr_flows;
uint32_t min_lcores;
uint32_t frame_size;
uint32_t tcp_payload_size;

// General variables
uint64_t TICKS_PER_US;
uint16_t *flow_indexes_array;
uint32_t *interarrival_array;
application_node_t *application_array;

// Heap and DPDK allocated
uint32_t incoming_idx;
node_t *incoming_array;
struct rte_mempool *pktmbuf_pool_rx;
struct rte_mempool *pktmbuf_pool_tx;
tcp_control_block_t *tcp_control_blocks;

// Internal threads variables
uint8_t quit_rx = 0;
uint8_t quit_tx = 0;
uint8_t quit_rx_ring = 0;
uint32_t nr_never_sent = 0;
struct rte_ring *rx_ring;

// Connection variables
uint16_t dst_tcp_port;
uint32_t dst_ipv4_addr;
uint32_t src_ipv4_addr;
struct rte_ether_addr dst_eth_addr;
struct rte_ether_addr src_eth_addr;

// Process the incoming TCP packet
int process_rx_pkt(struct rte_mbuf *pkt, node_t *incoming, uint32_t *incoming_idx) {
	// process only TCP packets
	struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
	if(unlikely(ipv4_hdr->next_proto_id != IPPROTO_TCP)) {
		return 0;
	}

	// get TCP header
	uint32_t ip_hdr_len = (ipv4_hdr->version_ihl & 0x0f)*4;
	struct rte_tcp_hdr *tcp_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_tcp_hdr *, sizeof(struct rte_ether_hdr) + ip_hdr_len);
	
	// get TCP payload size
	uint32_t tcp_hdr_len = ((tcp_hdr->data_off >> 4)*4);
	uint32_t packet_data_size = rte_be_to_cpu_16(ipv4_hdr->total_length) - ip_hdr_len - tcp_hdr_len;

	// do not process empty packets
	if(unlikely(packet_data_size == 0)) {
		return 0;
	}

	// obtain both timestamps from the packet
	uint64_t *payload = (uint64_t *)(((uint8_t*) tcp_hdr) + tcp_hdr_len);
	uint64_t t0 = payload[0];
	uint64_t t1 = payload[1];
	uint64_t f_id = payload[2];
	uint64_t w_id = payload[3];
	
	// retrieve the index of the flow from the NIC (NIC tags the packet according the 5-tuple using DPDK rte_flow)
	uint32_t flow_id = pkt->hash.fdir.hi;

	// sanity check
	if(unlikely(flow_id != f_id)) {
		return 0;
	}
	
	// get control block for the flow
	tcp_control_block_t *block = &tcp_control_blocks[flow_id];

	// update receive window from the packet
	rte_atomic16_set(&block->tcb_rwin, tcp_hdr->rx_win);

	// do not process retransmitted packets
	uint32_t seq = rte_be_to_cpu_32(tcp_hdr->sent_seq);
	if(likely(SEQ_LT(block->last_seq_recv, seq))) {
		block->last_seq_recv = seq;
	}

	// update ACK number in the TCP control block from the packet
	uint32_t ack_cur = rte_be_to_cpu_32(rte_atomic32_read(&block->tcb_next_ack));
	uint32_t ack_hdr = seq + packet_data_size;
	if(likely(SEQ_LEQ(ack_cur, ack_hdr))) {
		uint32_t acked = rte_cpu_to_be_32(ack_hdr);
		rte_atomic32_set(&block->tcb_next_ack, acked);
	}

	// fill the node previously allocated
	node_t *node = &incoming[(*incoming_idx)++];
	node->timestamp_tx = t0;
	node->timestamp_rx = t1;
	node->flow_id = f_id;
	node->worker_id = w_id;

	return 1;
}

// Start the client establishing all TCP connections
void start_client(uint16_t portid) {
	uint16_t nb_rx;
	uint16_t nb_tx;
	uint64_t ts_syn;
	struct rte_mbuf *pkt;
	struct rte_flow_error err;
	tcp_control_block_t *block;
	uint32_t nb_retransmission;
	struct rte_mbuf *pkts[BURST_SIZE];

	// flush all flow rules
	int ret = rte_flow_flush(portid, &err);
	if(ret != 0) {
		rte_exit(EXIT_FAILURE, "Cannot flush all rules associated with a port=%d\n", portid);
	}

	for(int i = 0; i < nr_flows; i++) {
		// get the TCP control block for the flow
		block = &tcp_control_blocks[i];
		// create the TCP SYN packet
		struct rte_mbuf *syn_packet = create_syn_packet(i);
		// insert the rte_flow in the NIC to retrieve the flow id for incoming packets of this flow
		insert_flow(portid, i);

		// send the SYN packet
		struct rte_mbuf *syn_cloned = rte_pktmbuf_clone(syn_packet, pktmbuf_pool_tx);
		nb_tx = rte_eth_tx_burst(portid, 0, &syn_cloned, 1);
		if(nb_tx != 1) {
			rte_exit(EXIT_FAILURE, "Error to send the TCP SYN packet.\n");
		}

		// clear the counters
		nb_retransmission = 1;
		ts_syn = rte_rdtsc();

		// change the TCP state to SYN_SENT
		rte_atomic16_set(&block->tcb_state, TCP_SYN_SENT);

		// while not received SYN+ACK packet and TCP state is not ESTABLISHED
		while(rte_atomic16_read(&block->tcb_state) != TCP_ESTABLISHED) {
			// receive TCP SYN+ACK packets from the NIC
			nb_rx = rte_eth_rx_burst(portid, 0, pkts, BURST_SIZE);

			for(int j = 0; j < nb_rx; j++) {
				// process the SYN+ACK packet, returning the ACK packet to send
				pkt = process_syn_ack_packet(pkts[j]);
				
				if(pkt) {
					// send the TCP ACK packet to the server
					nb_tx = rte_eth_tx_burst(portid, 0, &pkt, 1);
					if(nb_tx != 1) {
						rte_exit(EXIT_FAILURE, "Error to send the TCP ACK packet.\n");
					}
				}
			}
			// free packets
			rte_pktmbuf_free_bulk(pkts, nb_rx);

			if((rte_rdtsc() - ts_syn) > (nb_retransmission * HANDSHAKE_TIMEOUT_IN_US) * TICKS_PER_US) {
				nb_retransmission++;
				syn_cloned = rte_pktmbuf_clone(syn_packet, pktmbuf_pool_tx);
				nb_tx = rte_eth_tx_burst(portid, 0, &syn_cloned, 1);
				if(nb_tx != 1) {
					rte_exit(EXIT_FAILURE, "Error to send the TCP SYN packet.\n");
				}
				ts_syn = rte_rdtsc();

				if(nb_retransmission == HANDSHAKE_RETRANSMISSION) {
					rte_exit(EXIT_FAILURE, "Cannot establish connection.\n");
				}
			}
		}
		rte_pktmbuf_free(syn_packet);
	}

	// Discard 3-way handshake packets in the DPDK metrics
	rte_eth_stats_reset(portid);
	rte_eth_xstats_reset(portid);
	
	rte_compiler_barrier();
}

// RX processing
static int lcore_rx_ring(void *arg) {
	uint16_t nb_rx;
	struct rte_mbuf *pkts[BURST_SIZE];

	incoming_idx = 0;

	while(!quit_rx_ring) {
		// retrieve packets from the RX core
		nb_rx = rte_ring_sc_dequeue_burst(rx_ring, (void**) pkts, BURST_SIZE, NULL); 
		for(int i = 0; i < nb_rx; i++) {
			// process the incoming packet
			process_rx_pkt(pkts[i], incoming_array, &incoming_idx);
			// free the packet
			rte_pktmbuf_free(pkts[i]);
		}
	}

	// process all remaining packets that are in the RX ring (not from the NIC)
	do {
		nb_rx = rte_ring_sc_dequeue_burst(rx_ring, (void**) pkts, BURST_SIZE, NULL);
		for(int i = 0; i < nb_rx; i++) {
			// process the incoming packet
			process_rx_pkt(pkts[i], incoming_array, &incoming_idx);
			// free the packet
			rte_pktmbuf_free(pkts[i]);
		}
	} while (nb_rx != 0);

	return 0;
}

// Main RX processing
static int lcore_rx(void *arg) {
	uint16_t portid = 0;
	uint8_t qid = 0;

	uint64_t now;
	uint16_t nb_rx;
	uint16_t nb_pkts;
	struct rte_mbuf *pkts[BURST_SIZE];
	
	while(!quit_rx) {
		// retrieve the packets from the NIC
		nb_rx = rte_eth_rx_burst(portid, qid, pkts, BURST_SIZE);

		// retrive the current timestamp
		now = rte_rdtsc();
		for(int i = 0; i < nb_rx; i++) {
			// fill the timestamp into packet payload
			fill_payload_pkt(pkts[i], 1, now);
		}

		// enqueue the packets to the ring
		nb_pkts = rte_ring_sp_enqueue_burst(rx_ring, (void* const*) pkts, nb_rx, NULL);
		if(unlikely(nb_pkts != nb_rx)) {
			rte_exit(EXIT_FAILURE, "Cannot enqueue the packet to the RX thread: %s.\n", rte_strerror(errno));
		}
	}

	return 0;
}

// Main TX processing
static int lcore_tx(void *arg) {
	uint16_t portid = 0;
	uint8_t qid = 0;
	uint64_t nr_elements = rate * duration;

	struct rte_mbuf *pkt;

	uint64_t next_tsc = rte_rdtsc() + interarrival_array[0];

	for(uint64_t i = 0; i < nr_elements; i++) {
		// unable to keep up with the requested rate
		if(unlikely(rte_rdtsc() > (next_tsc + 5*TICKS_PER_US))) {
			// count this batch as dropped
			nr_never_sent++;
			next_tsc += (interarrival_array[i] + TICKS_PER_US);
			continue;
		}

		// choose the flow to send
		uint16_t flow_id = flow_indexes_array[i];
		tcp_control_block_t *block = &tcp_control_blocks[flow_id];

		// allocated the packet
		pkt = rte_pktmbuf_alloc(pktmbuf_pool_tx);

		// fill the packet fields
		fill_tcp_packet(block, pkt);

		// fill the timestamp, flow id, server iterations, and server randomness into the packet payload
		fill_payload_pkt(pkt, 0, next_tsc);
		fill_payload_pkt(pkt, 2, (uint64_t) flow_id);
		fill_payload_pkt(pkt, 4, application_array[i].iterations);
		fill_payload_pkt(pkt, 5, application_array[i].randomness);

		// check the receive window for this flow
		uint16_t rx_wnd = rte_atomic16_read(&block->tcb_rwin);
		while(unlikely(rx_wnd < tcp_payload_size)) { 
			rx_wnd = rte_atomic16_read(&block->tcb_rwin);
		}

		// sleep for while
		while (rte_rdtsc() < next_tsc) { }

		// fill the TCP ACK field
		hot_fill_tcp_packet(block, pkt);

		// send the packet
		rte_eth_tx_burst(portid, qid, &pkt, 1);

		// update the counter
		next_tsc += interarrival_array[i];
	}

	return 0;
}

// main function
int main(int argc, char **argv) {
	// init EAL
	int ret = rte_eal_init(argc, argv);
	if(ret < 0) {
		rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	}

	argc -= ret;
	argv += ret;

	// parse application arguments (after the EAL ones)
	ret = app_parse_args(argc, argv);
	if(ret < 0) {
		rte_exit(EXIT_FAILURE, "Invalid arguments\n");
	}

	// initialize DPDK
	uint16_t portid = 0;
	init_DPDK(portid, 1, seed);

	// create nodes for incoming packets
	create_incoming_array();

	// create flow indexes array
	create_flow_indexes_array();

	// create interarrival array
	create_interarrival_array();

	// create application array
	create_application_array();

	// initialize TCP control blocks
	init_tcp_blocks();

	// start client (3-way handshake for each flow)
	start_client(portid);

	// create the DPDK ring for RX thread
	create_dpdk_ring();

	// start RX thread to process incoming packets
	uint32_t id_lcore = rte_lcore_id();
	id_lcore = rte_get_next_lcore(id_lcore, 1, 1);
	rte_eal_remote_launch(lcore_rx_ring, NULL, id_lcore);

	// start RX thread to receive incoming packets
	id_lcore = rte_get_next_lcore(id_lcore, 1, 1);
	rte_eal_remote_launch(lcore_rx, NULL, id_lcore);

	// start TX thread
	id_lcore = rte_get_next_lcore(id_lcore, 1, 1);
	rte_eal_remote_launch(lcore_tx, NULL, id_lcore);

	// wait for duration parameter
	wait_timeout();

	// wait for RX/TX threads
	uint32_t lcore_id;
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if(rte_eal_wait_lcore(lcore_id) < 0) {
			return -1;
		}
	}

	// print stats
	print_stats_output();

	// print DPDK stats
	print_dpdk_stats(portid);

	// clean up
	clean_heap();
	clean_hugepages();

	return 0;
}
