#include "util.h"

int distribution;
char output_file[MAXSTRLEN];

// Sample the value using Exponential Distribution
double sample(double lambda) {
	double u = rte_drand();
	return -log(1 - u) / lambda;
}

// Convert string type into int type
static uint32_t process_int_arg(const char *arg) {
	char *end = NULL;

	return strtoul(arg, &end, 10);
}

uint64_t get_instructions_for_the_server(uint32_t *arr, uint32_t randomness) {
	uint32_t i = randomness % (PERCENTILES-1);
	double r = (double)randomness / RAND_MAX;

	uint32_t lb = arr[i];
	uint32_t ub = arr[i+1];

	uint32_t service_time = (lb + (r * (ub - lb)));
	
	return service_time / srv_time_in_ns_per_instruction;
}

uint64_t get_nr_packets(FILE *fp, uint32_t offset) {
	char buffer[MAXSTRLEN];

	// Skipping the first line
	char* ret __attribute__((unused)) = fgets(buffer, MAXSTRLEN, fp);

	// Skipping until reach to the 'offset' line
	for(uint32_t i = 0; i < offset; i++) {
		ret = fgets(buffer, MAXSTRLEN, fp);
	}

	// Couting the number of packets considering the duration
	uint64_t n = 0;
	for(uint32_t i = 0; i < 2*duration; i++) {
		ret = fgets(buffer, MAXSTRLEN, fp);
		char *token = strtok(buffer, ",");
		for(uint32_t j = 0; j < (PERCENTILES+1); j++) {
			token = strtok(NULL, ",");
		}
		token = strtok(NULL, ",");
		n += atoi(token);
	}

	return n;
}

inline void process() {
	char *ret __attribute__((unused));
	ret = strtok(NULL, ",");

	// Get the percentiles, choose one, and update the application_array
	uint32_t arr[PERCENTILES];
	for(uint32_t i = 0; i < PERCENTILES; i++) {
		arr[i] = atoi(ret);
		ret = strtok(NULL, ",");
	}
	
	// Get the number of packets 
	ret = strtok(NULL, ","); // Skip the QPS
	uint32_t queries = atoi(ret);
	double mean = (1.0/queries) * 1000000.0;

	// Distributed the packets uniformly within 1-sec window
	// Distributed the service time uniformly
	for(uint32_t i = 0; i < queries; i++) {
		uint32_t j = rand();
		application_array[idx].instructions = get_instructions_for_the_server(arr, j);
		application_array[idx].randomness = j;
		interarrival_array[idx] = mean * TICKS_PER_US;
		idx++;
	}
}

// Process the CSV file, creating the auxiliary structures
void process_csv_file() {
	char buffer[MAXSTRLEN];
	FILE* fp = fopen(csv_filename, "r");
	if(!fp) {
		fprintf(stderr, "Error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if(srv_application == SQRT_APPLICATION_VALUE) {
		srv_time_in_ns_per_instruction = sqrt_time_one_iteration;
	} else if(srv_application == STRIDEDMEM_APPLICATION_VALUE) {
		srv_time_in_ns_per_instruction = stridedmem_time_one_iteration;
	} else if(srv_application == NULL_APPLICATION_VALUE) {
		srv_time_in_ns_per_instruction = null_time_one_iteration;
	}

	idx = 0;
	uint32_t offset = csv_offset;
	nr_packets = get_nr_packets(fp, offset);
	rewind(fp);

	create_incoming_array();
	create_flow_indexes_array();

	// Allocates an array for all outgoing packets
	interarrival_array = (uint32_t*) rte_malloc(NULL, nr_packets * sizeof(uint32_t), 64);
	if(interarrival_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the interarrival_gap array.\n");
	}
	nr_never_sent = 0;

	// Allocates an array for the service time
	application_array = (application_node_t*) rte_malloc(NULL, nr_packets * sizeof(application_node_t), 64);
	if(application_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the application array.\n");
	}

	// Skipping the first line
	char* ret __attribute__((unused)) = fgets(buffer, MAXSTRLEN, fp);

	// Skipping until reach to the 'offset' line
	for(uint32_t i = 0; i < offset; i++) {
		ret = fgets(buffer, MAXSTRLEN, fp);
	}

	// 1st iteration
	// Read the line
	ret = fgets(buffer, MAXSTRLEN, fp);
	// Tokenizer the buffer
	char *token = strtok(buffer, ",");
	// Store the first time
	strcpy(csv_start_time, token);
	// Process the first entry
	process();

	for(uint32_t i = 1; i < 2*duration - 1; i++) {
		ret = fgets(buffer, MAXSTRLEN, fp);
		ret = strtok(buffer, ",");
		process();
	}

	// Last iteration
	ret = fgets(buffer, MAXSTRLEN, fp);
	token = strtok(buffer, ",");
	strcpy(csv_end_time, token);
	process();
}

// Allocate nodes for all incoming packets
void create_incoming_array() {
	incoming_array = (node_t*) rte_malloc(NULL, nr_packets * 1.4 * sizeof(node_t), 0);
	if(incoming_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the incoming array.\n");
	}
} 

// Allocate and create an array for all flow indentier to send to the server
void create_flow_indexes_array() {
	flow_indexes_array = (uint16_t*) rte_malloc(NULL, nr_packets * sizeof(uint16_t), 0);
	if(flow_indexes_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the flow_indexes array.\n");
	}

	for(uint64_t i = 0; i < nr_packets; i++) {
		flow_indexes_array[i] = i % nr_flows;
	}
}

// Clean up all allocate structures
void clean_heap() {
	rte_free(incoming_array);
	rte_free(flow_indexes_array);
	rte_free(interarrival_array);
	rte_free(application_array);
}

// Usage message
static void usage(const char *prgname) {
	printf("%s [EAL options] -- \n"
		"  -f FLOWS: number of flows\n"
		"  -s SIZE: frame size in bytes\n"
		"  -t TIME: time in seconds to send packets\n"
		"  -e SEED: seed\n"
		"  -a APPLICATION: <sqrt|stridedmem|null> on the server\n"
		"  -i OFFSET: offset of the CSV file\n"
		"  -c FILENAME: name of the configuration file\n"
		"  -o FILENAME: name of the output file\n"
		"  -C FILENAME: name of the CSV file\n",
		prgname
	);
}

// Parse the argument given in the command line of the application
int app_parse_args(int argc, char **argv) {
	int opt, ret;
	char **argvopt;
	char *prgname = argv[0];

	argvopt = argv;
	while ((opt = getopt(argc, argvopt, "a:f:s:t:c:C:o:e:i:")) != EOF) {
		switch (opt) {
		// offset of the CSV file
		case 'i':
			csv_offset = process_int_arg(optarg);
			break;
		
		// flows
		case 'f':
			nr_flows = process_int_arg(optarg);
			break;

		// frame size (bytes)
		case 's':
			frame_size = process_int_arg(optarg);
			if (frame_size < MIN_PKTSIZE) {
				rte_exit(EXIT_FAILURE, "The minimum packet size is %d.\n", MIN_PKTSIZE);
			}
			tcp_payload_size = (frame_size - sizeof(struct rte_ether_hdr) - sizeof(struct rte_ipv4_hdr) - sizeof(struct rte_tcp_hdr));
			break;

		// duration (s)
		case 't':
			duration = process_int_arg(optarg);
			break;
		
		// seed
		case 'e':
			seed = process_int_arg(optarg);
			break;

		// server's application
		case 'a':
			if(strcmp(optarg, "sqrt") == 0) {
				// Square root
				srv_application = SQRT_APPLICATION_VALUE;
			} else if(strcmp(optarg, "stridedmem") == 0) {
				// Stridedmem
				srv_application = STRIDEDMEM_APPLICATION_VALUE;
			} else if(strcmp(optarg, "null") == 0) {
				// Null (busy waiting)
				srv_application = NULL_APPLICATION_VALUE;
			} else {
				usage(prgname);
				rte_exit(EXIT_FAILURE, "Invalid arguments.\n");
			}
			break;

		// config file name
		case 'c':
			process_config_file(optarg);
			break;
		
		// CSV file
		case 'C':
			strcpy(csv_filename, optarg);
			break;

		// output mode
		case 'o':
			strcpy(output_file, optarg);
			break;

		default:
			usage(prgname);
			rte_exit(EXIT_FAILURE, "Invalid arguments.\n");
		}
	}

	if(optind >= 0) {
		argv[optind - 1] = prgname;
	}

	ret = optind-1;
	optind = 1;

	return ret;
}

// Wait for the duration parameter
void wait_timeout() {
	uint32_t remaining_in_s = 5;
	rte_delay_us_sleep((2 * duration + remaining_in_s) * 1000000);

	// set quit flag for all internal cores
	quit_rx = 1;
	quit_tx = 1;
	quit_rx_ring = 1;
}

// Compare two double values (for qsort function)
int cmp_func(const void * a, const void * b) {
	double da = (*(double*)a);
	double db = (*(double*)b);

	return (da - db) > ( (fabs(da) < fabs(db) ? fabs(db) : fabs(da)) * EPSILON);
}

// Print stats into output file
void print_stats_output() {
	// open the file
	FILE *fp = fopen(output_file, "w");
	if(fp == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot open the output file.\n");
	}

	uint64_t total_never_sent = nr_never_sent;
	if((incoming_idx + total_never_sent) != nr_packets) {
		printf("ERROR: received %d and %ld never sent\n", incoming_idx, total_never_sent);
		fclose(fp);
		return;
	}

	printf("\nStart/End -- %s -- %s\n", csv_start_time, csv_end_time);
	printf("incoming_idx = %d -- never_sent = %ld\n", incoming_idx, total_never_sent);
	uint64_t j = nr_packets/2;

	// print the RTT latency in (ns)
	node_t *cur;
	for(; j < incoming_idx; j++) {
		cur = &incoming_array[j];

		fprintf(fp, "%lu\t%lu\t0x%02lx\n", 
			((uint64_t)((cur->timestamp_rx - cur->timestamp_tx)/((double)TICKS_PER_US/1000))),
			cur->flow_id,
			cur->worker_id
		);
	}

	// close the file
	fclose(fp);
}

// Process the config file
void process_config_file(char *cfg_file) {
	// open the file
	struct rte_cfgfile *file = rte_cfgfile_load(cfg_file, 0);
	if(file == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot load configuration profile %s\n", cfg_file);
	}

	// load ethernet addresses
	char *entry = (char*) rte_cfgfile_get_entry(file, "ethernet", "src");
	if(entry) {
		rte_ether_unformat_addr((const char*) entry, &src_eth_addr);
	}
	entry = (char*) rte_cfgfile_get_entry(file, "ethernet", "dst");
	if(entry) {
		rte_ether_unformat_addr((const char*) entry, &dst_eth_addr);
	}

	// load ipv4 addresses
	entry = (char*) rte_cfgfile_get_entry(file, "ipv4", "src");
	if(entry) {
		uint8_t b3, b2, b1, b0;
		sscanf(entry, "%hhd.%hhd.%hhd.%hhd", &b3, &b2, &b1, &b0);
		src_ipv4_addr = IPV4_ADDR(b3, b2, b1, b0);
	}
	entry = (char*) rte_cfgfile_get_entry(file, "ipv4", "dst");
	if(entry) {
		uint8_t b3, b2, b1, b0;
		sscanf(entry, "%hhd.%hhd.%hhd.%hhd", &b3, &b2, &b1, &b0);
		dst_ipv4_addr = IPV4_ADDR(b3, b2, b1, b0);
	}

	// load TCP destination port
	entry = (char*) rte_cfgfile_get_entry(file, "tcp", "dst");
	if(entry) {
		uint16_t port;
		sscanf(entry, "%hu", &port);
		dst_tcp_port = port;
	}

	// load server calibration
	entry = (char*) rte_cfgfile_get_entry(file, "application", "sqrt");
	if(entry) {
		double duration;
		sscanf(entry, "%lf", &duration);
		sqrt_time_one_iteration = duration;
	}
	entry = (char*) rte_cfgfile_get_entry(file, "application", "stridedmem");
	if(entry) {
		double duration;
		sscanf(entry, "%lf", &duration);
		stridedmem_time_one_iteration = duration;
	}
	entry = (char*) rte_cfgfile_get_entry(file, "application", "null");
	if(entry) {
		double duration;
		sscanf(entry, "%lf", &duration);
		null_time_one_iteration = duration;
	}

	// close the file
	rte_cfgfile_close(file);
}

// Fill the data into packet payload properly
inline void fill_payload_pkt(struct rte_mbuf *pkt, uint32_t i, uint64_t value) {
	uint8_t *payload = (uint8_t*) rte_pktmbuf_mtod_offset(pkt, uint8_t*, PAYLOAD_OFFSET);

	((uint64_t*) payload)[i] = value;
}
