#include "tcp_util.h"

// Create and initialize the TCP Control Blocks for all flows
void init_tcp_blocks() {
	// allocate the all control block structure previosly
	tcp_control_blocks = (tcp_control_block_t *) rte_zmalloc("tcp_control_blocks", nr_flows * sizeof(tcp_control_block_t), RTE_CACHE_LINE_SIZE);

	// choose TCP source port for all flows
	uint16_t src_tcp_port;
	uint16_t src_ports[nr_flows];
	for(uint32_t i = 0; i < nr_flows; i++) {
		src_ports[i] = rte_cpu_to_be_16((i % nr_flows) + 1);
	}

	for(uint32_t i = 0; i < nr_flows; i++) {
		rte_atomic16_init(&tcp_control_blocks[i].tcb_state);
		rte_atomic16_set(&tcp_control_blocks[i].tcb_state, TCP_INIT);
		rte_atomic16_set(&tcp_control_blocks[i].tcb_rwin, 0xFFFF);

		src_tcp_port = src_ports[i];

		tcp_control_blocks[i].src_addr = src_ipv4_addr;
		tcp_control_blocks[i].dst_addr = dst_ipv4_addr;

		tcp_control_blocks[i].src_port = src_tcp_port;
		tcp_control_blocks[i].dst_port = rte_cpu_to_be_16(dst_tcp_port);

		uint32_t seq = rte_rand();
		tcp_control_blocks[i].tcb_seq_ini = seq;
		tcp_control_blocks[i].tcb_next_seq = seq;

		tcp_control_blocks[i].flow_mark_action.id = i;
		tcp_control_blocks[i].flow_queue_action.index = 0;
		tcp_control_blocks[i].flow_eth.type = ETH_IPV4_TYPE_NETWORK;
		tcp_control_blocks[i].flow_eth_mask.type = 0xFFFF;
		tcp_control_blocks[i].flow_ipv4.hdr.src_addr = tcp_control_blocks[i].dst_addr;
		tcp_control_blocks[i].flow_ipv4.hdr.dst_addr = tcp_control_blocks[i].src_addr;
		tcp_control_blocks[i].flow_ipv4_mask.hdr.src_addr = 0xFFFFFFFF;
		tcp_control_blocks[i].flow_ipv4_mask.hdr.dst_addr = 0xFFFFFFFF;
		tcp_control_blocks[i].flow_tcp.hdr.src_port = tcp_control_blocks[i].dst_port;
		tcp_control_blocks[i].flow_tcp.hdr.dst_port = tcp_control_blocks[i].src_port;
		tcp_control_blocks[i].flow_tcp_mask.hdr.src_port = 0xFFFF;
		tcp_control_blocks[i].flow_tcp_mask.hdr.dst_port = 0xFFFF;
	}
}

// Create the TCP SYN packet
struct rte_mbuf* create_syn_packet(uint16_t i) {
	// allocate TCP SYN packet in the hugepages
	struct rte_mbuf* pkt = rte_pktmbuf_alloc(pktmbuf_pool_tx);
	if(pkt == NULL) {
		rte_exit(EXIT_FAILURE, "Error to alloc a rte_mbuf.\n");
	}

	// ensure that IP/TCP checksum offloadings
	pkt->ol_flags |= (RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_TCP_CKSUM);

	// get control block for the flow
	tcp_control_block_t *block = &tcp_control_blocks[i];

	// fill Ethernet information
	struct rte_ether_hdr *eth_hdr = (struct rte_ether_hdr *) rte_pktmbuf_mtod(pkt, struct ether_hdr*);
	eth_hdr->dst_addr = dst_eth_addr;
	eth_hdr->src_addr = src_eth_addr;
	eth_hdr->ether_type = ETH_IPV4_TYPE_NETWORK;

	// fill IPv4 information
	struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
	ipv4_hdr->version_ihl = 0x45;
	ipv4_hdr->total_length = rte_cpu_to_be_16(sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_tcp_hdr) + sizeof(tcp_options_ws_t) + sizeof(tcp_options_mss_t));
	ipv4_hdr->time_to_live = 255;
	ipv4_hdr->packet_id = 0;
	ipv4_hdr->next_proto_id = IPPROTO_TCP;
	ipv4_hdr->fragment_offset = 0;
	ipv4_hdr->src_addr = block->src_addr;
	ipv4_hdr->dst_addr = block->dst_addr;
	ipv4_hdr->hdr_checksum = 0;

	// fill TCP information
	struct rte_tcp_hdr *tcp_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_tcp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
	tcp_hdr->src_port = block->src_port;
	tcp_hdr->dst_port = block->dst_port;
	tcp_hdr->sent_seq = block->tcb_seq_ini;
	tcp_hdr->recv_ack = 0;
	tcp_hdr->data_off = ((sizeof(struct rte_tcp_hdr) + sizeof(tcp_options_ws_t) + sizeof(tcp_options_mss_t)) >> 2) << 4;
	tcp_hdr->tcp_flags = RTE_TCP_SYN_FLAG;
	tcp_hdr->rx_win = 0xFFFF;
	tcp_hdr->cksum = 0;
	tcp_hdr->tcp_urp = 0;

	tcp_options_ws_t *tcp_ws = (tcp_options_ws_t*)(((uint8_t*) tcp_hdr) + sizeof(struct rte_tcp_hdr));
	tcp_ws->kind = 0x03;
	tcp_ws->length = 0x03;
	tcp_ws->shift = 0x0a;
	tcp_ws->nop = 0x01;
	
	tcp_options_mss_t *tcp_mss = (tcp_options_mss_t*)(((uint8_t*) tcp_ws) + sizeof(tcp_options_ws_t));
	tcp_mss->kind = 0x02;
	tcp_mss->length = 0x04;
	tcp_mss->value = htons(65535);

	// fill the packet size
	pkt->data_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_tcp_hdr) + sizeof(tcp_options_ws_t) + sizeof(tcp_options_mss_t);
	pkt->pkt_len = pkt->data_len;

	return pkt;
}

// Create the TCP ACK packet
struct rte_mbuf *create_ack_packet(uint16_t i) {
	// allocate TCP ACK packet in the hugepages
	struct rte_mbuf* pkt = rte_pktmbuf_alloc(pktmbuf_pool_tx);
	if(pkt == NULL) {
		rte_exit(EXIT_FAILURE, "Error to alloc a rte_mbuf.\n");
	}

	// ensure that IP/TCP checksum offloadings
	pkt->ol_flags |= (RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_TCP_CKSUM);

	// get control block for the flow
	tcp_control_block_t *block = &tcp_control_blocks[i];

	// fill Ethernet information
	struct rte_ether_hdr *eth_hdr = (struct rte_ether_hdr *) rte_pktmbuf_mtod(pkt, struct ether_hdr*);
	eth_hdr->dst_addr = dst_eth_addr;
	eth_hdr->src_addr = src_eth_addr;
	eth_hdr->ether_type = ETH_IPV4_TYPE_NETWORK;

	// fill IPv4 information
	struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
	ipv4_hdr->version_ihl = 0x45;
	ipv4_hdr->total_length = rte_cpu_to_be_16(sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_tcp_hdr));
	ipv4_hdr->time_to_live = 255;
	ipv4_hdr->packet_id = 0;
	ipv4_hdr->next_proto_id = IPPROTO_TCP;
	ipv4_hdr->fragment_offset = 0;
	ipv4_hdr->src_addr = block->src_addr;
	ipv4_hdr->dst_addr = block->dst_addr;
	ipv4_hdr->hdr_checksum = 0;

	// set the TCP SEQ number
	uint32_t newseq = rte_cpu_to_be_32(rte_be_to_cpu_32(block->tcb_next_seq) + 1);
	block->tcb_next_seq = newseq;

	// fill TCP information
	struct rte_tcp_hdr *tcp_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_tcp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
	tcp_hdr->src_port = block->src_port;
	tcp_hdr->dst_port = block->dst_port;
	tcp_hdr->sent_seq = newseq;
	tcp_hdr->recv_ack = rte_atomic32_read(&block->tcb_next_ack);
	tcp_hdr->data_off = (sizeof(struct rte_tcp_hdr) >> 2) << 4;
	tcp_hdr->tcp_flags = RTE_TCP_ACK_FLAG;
	tcp_hdr->rx_win = 0xFFFF;
	tcp_hdr->cksum = 0;
	tcp_hdr->tcp_urp = 0;

	// fill the packet size
	pkt->data_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_tcp_hdr);
	pkt->pkt_len = pkt->data_len;

	return pkt;
}

// Process the TCP SYN+ACK packet and return the TCP ACK
struct rte_mbuf* process_syn_ack_packet(struct rte_mbuf* pkt) {
	// process only IPv4 packets
	struct rte_ether_hdr *eth_hdr = (struct rte_ether_hdr *) rte_pktmbuf_mtod(pkt, struct ether_hdr*);
	if(eth_hdr->ether_type != ETH_IPV4_TYPE_NETWORK) {
		return NULL;
	}

	// process only TCP packets
	struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
	if(ipv4_hdr->next_proto_id != IPPROTO_TCP) {
		return NULL;
	}

	// get TCP header
	struct rte_tcp_hdr *tcp_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_tcp_hdr *, sizeof(struct rte_ether_hdr) + (ipv4_hdr->version_ihl & 0x0f)*4);

	// retrieve the index of the flow from the NIC (NIC tags the packet according the 5-tuple using DPDK rte_flow)
	uint32_t idx = pkt->hash.fdir.hi;

	// get control block for the flow
	tcp_control_block_t *block = &tcp_control_blocks[idx];

	// get the TCP control block state
	uint8_t state = rte_atomic16_read(&block->tcb_state);

	// process only in SYN_SENT state and SYN+ACK packet
	if((state == TCP_SYN_SENT) && (tcp_hdr->tcp_flags == (RTE_TCP_SYN_FLAG|RTE_TCP_ACK_FLAG))) {
		// update the TCP state to ESTABLISHED
		rte_atomic16_set(&block->tcb_state, TCP_ESTABLISHED);

		// get the TCP SEQ number
		uint32_t seq = rte_be_to_cpu_32(tcp_hdr->sent_seq);
		block->last_seq_recv = seq;

		// update TCP SEQ and ACK numbers
		rte_atomic32_set(&block->tcb_next_ack, rte_cpu_to_be_32(seq + 1));
		block->tcb_ack_ini = tcp_hdr->sent_seq;

		// return TCP ACK packet
		return create_ack_packet(idx);
	}

	return NULL;
}

// Fill the TCP packets from TCP Control Block data
void fill_tcp_packet(tcp_control_block_t *block, struct rte_mbuf *pkt) {
	// ensure that IP/TCP checksum offloadings
	pkt->ol_flags |= (RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_TCP_CKSUM);

	// fill Ethernet information
	struct rte_ether_hdr *eth_hdr = (struct rte_ether_hdr *) rte_pktmbuf_mtod(pkt, struct ether_hdr*);
	eth_hdr->dst_addr = dst_eth_addr;
	eth_hdr->src_addr = src_eth_addr;
	eth_hdr->ether_type = ETH_IPV4_TYPE_NETWORK;

	// fill IPv4 information
	struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
	ipv4_hdr->version_ihl = 0x45;
	ipv4_hdr->total_length = rte_cpu_to_be_16(frame_size - sizeof(struct rte_ether_hdr));
	ipv4_hdr->time_to_live = 255;
	ipv4_hdr->packet_id = 0;
	ipv4_hdr->next_proto_id = IPPROTO_TCP;
	ipv4_hdr->fragment_offset = 0;
	ipv4_hdr->src_addr = block->src_addr;
	ipv4_hdr->dst_addr = block->dst_addr;
	ipv4_hdr->hdr_checksum = 0;

	// set the TCP SEQ number
	uint32_t sent_seq = block->tcb_next_seq;

	// fill TCP information
	struct rte_tcp_hdr *tcp_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_tcp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
	tcp_hdr->dst_port = block->dst_port;
	tcp_hdr->src_port = block->src_port;
	tcp_hdr->sent_seq = sent_seq;
	tcp_hdr->recv_ack = rte_atomic32_read(&block->tcb_next_ack);
	tcp_hdr->data_off = (sizeof(struct rte_tcp_hdr) >> 2) << 4;
	tcp_hdr->tcp_flags = RTE_TCP_PSH_FLAG|RTE_TCP_ACK_FLAG;
	tcp_hdr->rx_win = 0xFFFF;
	tcp_hdr->cksum = 0;
	tcp_hdr->tcp_urp = 0;

	// updates the TCP SEQ number
	sent_seq = rte_cpu_to_be_32(rte_be_to_cpu_32(sent_seq) + tcp_payload_size);
	block->tcb_next_seq = sent_seq;

	// fill the packet size
	pkt->data_len = frame_size;
	pkt->pkt_len = pkt->data_len;
}

void hot_fill_tcp_packet(tcp_control_block_t *block, struct rte_mbuf *pkt) {
	// get IPv4 information
	struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));

	// fill TCP information
	struct rte_tcp_hdr *tcp_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_tcp_hdr *, sizeof(struct rte_ether_hdr) + (ipv4_hdr->version_ihl & 0x0f)*4);

	// fill TCP ACK information
	tcp_hdr->recv_ack = rte_atomic32_read(&block->tcb_next_ack);
}
