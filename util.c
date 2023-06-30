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

// Convert string type into double type
static double process_double_arg(const char *arg) {
	char *end;

	return strtod(arg, &end);
}

uint32_t get_nr_lines(FILE *fp) {
	char buffer[MAXSTRLEN];

	// Skipping the first line
	char* ret __attribute__((unused)) = fgets(buffer, MAXSTRLEN, fp);

	// Counting the number of lines
	uint32_t nr_lines = 0;
	while(fgets(buffer, MAXSTRLEN, fp)) {
		nr_lines++;
	}

	return nr_lines;
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
	uint64_t nr_packets = 0;
	for(uint32_t i = 0; i < 2*duration; i++) {
		ret = fgets(buffer, MAXSTRLEN, fp);
		char *token = strtok(buffer, ",");
		for(uint32_t j = 0; j < 12; j++) {
			token = strtok(NULL, ",");
		}
		token = strtok(NULL, ",");
		nr_packets += atoi(token);
	}

	return nr_packets;
}

inline void process(char *token, uint32_t skip_first) {
	char *ret __attribute__((unused));
	if(skip_first == 1)
		ret = strtok(NULL, ",");
	
	uint32_t arr[PERCENTILES];
	for(uint32_t i = 0; i < PERCENTILES; i++) {
		ret = strtok(NULL, ",");
		arr[i] = atoi(ret);
	}

	uint32_t i = rand() % PERCENTILES;
	printf("We choose = %d", arr[i]);

	// In this point, the 
}

// Process the CSV file, creating the auxiliary structures
void process_csv_file() {
	char buffer[MAXSTRLEN];
	FILE* fp = fopen(csv_filename, "r");
	if(!fp) {
		fprintf(stderr, "Error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	uint32_t nr_csv_lines = get_nr_lines(fp);
	rewind(fp);

	// Here I need to create the array of application and the incoming based of the duration and packets
	// create_flow_indexes_array(); aqui tem que saber o fluxo baseado na quantidade de pacotes que hoje eh unknown
	// create_interarrival_array(); // isso tem que ser criado junto
	// create_application_array();


	uint32_t offset = rand() % (nr_csv_lines - 2*duration);
	nr_packets = get_nr_packets(fp, offset);
	rewind(fp);

	create_incoming_array();
	create_flow_indexes_array();

	// Allocates an array for all outgoing packets

	printf("%d %d\n", nr_csv_lines, nr_packets);

	// Skipping the first line
	char* ret __attribute__((unused)) = fgets(buffer, MAXSTRLEN, fp);

	// Skipping until reach to the 'offset' line
	for(uint32_t i = 0; i < offset; i++) {
		ret = fgets(buffer, MAXSTRLEN, fp);
	}

	// Structure of the CSV file
	// Time,P0,P10,P20,P30,P40,P50,P60,P70,P80,P90,P100,QPS,Queries,Load

	// 1st iteration
	// Read the line
	ret = fgets(buffer, MAXSTRLEN, fp);
	// Tokenizer the buffer
	char *token = strtok(buffer, ",");
	// Store the first time
	strcpy(csv_start_time, token);
	// Process the first entry
	process(token, 0);

	// // Go on
	// for(uint32_t i = 1; i < 2*duration - 1; i++) {
	// 	ret = fgets(buffer, MAXSTRLEN, fp);

	// 	// Tokenizer the buffer
	// 	char *token = strtok(buffer, ",");

	// 	//
	// }
}

// Allocate and create all application nodes
void create_application_array() {
	uint64_t rate_per_queue = rate/nr_queues;
	uint64_t nr_elements_per_queue = (2 * rate_per_queue * duration);

	application_array = (application_node_t**) rte_malloc(NULL, nr_queues * sizeof(application_node_t*), 64);
	if(application_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the application array.\n");
	}

	for(uint64_t i = 0; i < nr_queues; i++) {
		application_array[i] = (application_node_t*) rte_malloc(NULL, nr_elements_per_queue * sizeof(application_node_t), 0);
		if(application_array[i] == NULL) {
			rte_exit(EXIT_FAILURE, "Cannot alloc the application array.\n");
		}

		if(srv_distribution == CONSTANT_VALUE) {
			for(uint32_t j = 0; j < nr_elements_per_queue; j++) {
				application_array[i][j].iterations = srv_iterations0;
				application_array[i][j].randomness = rte_rand();
			}
		} else if(srv_distribution == EXPONENTIAL_VALUE) {
			for(uint32_t j = 0; j < nr_elements_per_queue; j++) {
				double u = rte_drand();
				application_array[i][j].iterations = (uint64_t) (-((double)srv_iterations0) * log(u));
				application_array[i][j].randomness = rte_rand();
			}
		} else {
			for(uint32_t j = 0; j < nr_elements_per_queue; j++) {
				double u = rte_drand();
				if(u < srv_mode) {
					application_array[i][j].iterations = srv_iterations0;
				} else {
					application_array[i][j].iterations = srv_iterations1;
				}
			}
		}
	}
}

// Allocate nodes for all incoming packets
void create_incoming_array() {
	incoming_array = (node_t*) rte_malloc(NULL, nr_packets * 1.4 * sizeof(node_t), 0);
	if(incoming_array == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot alloc the incoming array.\n");
	}
} 

// Allocate an array for all interarrival packets
// void create_interarrival_array() {
// 	interarrival_array = (uint32_t**) rte_malloc(NULL, nr_queues * sizeof(uint32_t*), 64);
// 	if(interarrival_array == NULL) {
// 		rte_exit(EXIT_FAILURE, "Cannot alloc the interarrival_gap array.\n");
// 	}

// 	nr_never_sent = (uint32_t*) rte_zmalloc(NULL, nr_queues * sizeof(uint32_t), 64);
// 	if(nr_never_sent == NULL) {
// 		rte_exit(EXIT_FAILURE, "Cannot alloc the nr_never_sent array.\n");
// 	}

// 	for(uint64_t i = 0; i < nr_queues; i++) {
// 		interarrival_array[i] = (uint32_t*) rte_malloc(NULL, nr_elements_per_queue * sizeof(uint32_t), 0);
// 		if(interarrival_array[i] == NULL) {
// 			rte_exit(EXIT_FAILURE, "Cannot alloc the interarrival_gap array.\n");
// 		}
		
// 		uint32_t *interarrival_gap = interarrival_array[i];
// 		if(distribution == UNIFORM_VALUE) {
// 			double mean = (1.0/rate_per_queue) * 1000000.0;
// 			for(uint64_t j = 0; j < nr_elements_per_queue; j++) {
// 				interarrival_gap[j] = mean * TICKS_PER_US;
// 			}
// 		} else {
// 			double lambda = 1.0/(1000000.0/rate_per_queue);
// 			for(uint64_t j = 0; j < nr_elements_per_queue; j++) {
// 				interarrival_gap[j] = sample(lambda) * TICKS_PER_US;
// 			}
// 		}
// 	} 
// }

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
		"  -d DISTRIBUTION: <uniform|exponential>\n"
		"  -r RATE: rate in pps\n"
		"  -f FLOWS: number of flows\n"
		"  -s SIZE: frame size in bytes\n"
		"  -t TIME: time in seconds to send packets\n"
		"  -e SEED: seed\n"
		"  -D DISTRIBUTION: <uniform|exponential|bimodal> on the server\n"
		"  -i INSTRUCTIONS: number of instructions on the server\n"
		"  -j INSTRUCTIONS: number of instructions on the server\n"
		"  -m MODE: mode for Bimodal distribution\n"
		"  -c FILENAME: name of the configuration file\n"
		"  -o FILENAME: name of the output file\n",
		prgname
	);
}

// Parse the argument given in the command line of the application
int app_parse_args(int argc, char **argv) {
	int opt, ret;
	char **argvopt;
	char *prgname = argv[0];

	argvopt = argv;
	while ((opt = getopt(argc, argvopt, "d:r:f:s:p:t:c:C:o:e:D:i:j:m:")) != EOF) {
		switch (opt) {
		// distribution on the client
		case 'd':
			if(strcmp(optarg, "uniform") == 0) {
				// Uniform distribution
				distribution = UNIFORM_VALUE;
			} else if(strcmp(optarg, "exponential") == 0) {
				// Exponential distribution
				distribution = EXPONENTIAL_VALUE;
			} else {
				usage(prgname);
				rte_exit(EXIT_FAILURE, "Invalid arguments.\n");
			}
			break;

		// distribution on the server
		case 'D':
			if(strcmp(optarg, "constant") == 0) {
				// Constant
				srv_distribution = CONSTANT_VALUE;
			} else if(strcmp(optarg, "exponential") == 0) {
				// Exponential distribution
				srv_distribution = EXPONENTIAL_VALUE;
			} else if(strcmp(optarg, "bimodal") == 0) {
				// Bimodal distribution
				srv_distribution = BIMODAL_VALUE;
			} else {
				usage(prgname);
				rte_exit(EXIT_FAILURE, "Invalid arguments.\n");
			}
			break;
			
		// iterations on the server
		case 'i':
			srv_iterations0 = process_int_arg(optarg);
			break;
		
		// iterations on the server
		case 'j':
			srv_iterations1 = process_int_arg(optarg);
			break;

		// mode on the server
		case 'm':
			srv_mode = process_double_arg(optarg);
			break;

		// rate (pps)
		case 'r':
			rate = process_int_arg(optarg);
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

	if(nr_flows < nr_queues) {
		rte_exit(EXIT_FAILURE, "The number of flows should be bigger than the number of queues.\n");
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

	uint64_t total_never_sent = 0;
	for(uint32_t i = 0; i < nr_queues; i++) {
		total_never_sent += nr_never_sent[i];
	}
	if((incoming_idx + total_never_sent) != 2 * rate * duration) {
		printf("ERROR: received %d and %ld never sent\n", incoming_idx, total_never_sent);
		fclose(fp);
		return;
	}

	printf("\nincoming_idx = %d -- never_sent = %ld\n", incoming_idx, total_never_sent);
	uint64_t j = rate * duration - total_never_sent;

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

	// close the file
	rte_cfgfile_close(file);
}

// Fill the data into packet payload properly
inline void fill_payload_pkt(struct rte_mbuf *pkt, uint32_t idx, uint64_t value) {
	uint8_t *payload = (uint8_t*) rte_pktmbuf_mtod_offset(pkt, uint8_t*, PAYLOAD_OFFSET);

	((uint64_t*) payload)[idx] = value;
}
