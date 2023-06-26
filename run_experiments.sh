#!/bin/bash

RUNS=0
PERCENTILE_1="50.0"
PERCENTILE_2="99.9"

SERVER_TIMEOUT=35
SERVER_IP=198.19.2.1
SERVER_LAYOUT_LIST=("l1" "l2" "l3" "l4")
SERVER_SEED_LIST=(1646203793)
SERVER_APPLICATION="sqrt"
SERVER_CORES="0,2,4,6,8,10,12,14,16,18,22,24,26,28,30,32,34,36,38"
SERVER_APPLICATION_ITERATIONS_1=0 	## Need to calibrate to correct service time
SERVER_APPLICATION_ITERATIONS_2=0 	## For bimodal
SERVER_APPLICATION_DISTRIBUTION="constant"

CLIENT_INTERARRIVAL="exponential"
CLIENT_DURATION=10
CLIENT_RATE_INITIAL=50000
CLIENT_RATE_INCREMENTAL=50000
CLIENT_PCI_NIC="37:00.0"
CLIENT_CORES="2,4,6,8"
CLIENT_FLOWS=128
CLIENT_SIZE=128
CLIENT_QUEUES=1
CLIENT_CONF_FILE="addr.cfg"
CLIENT_OUTPUT_FILE="output.dat"

TIMEOUT=$(( SERVER_TIMEOUT - CLIENT_DURATION - CLIENT_DURATION ))

error () {
	local Z=1.96
	local N=`wc -l $1 | cut -d' ' -f1`

	MEAN=`awk '{sum += $1} END {printf "%f", (sum/NR)}' $1`
	STDEV=`awk '{sum += ($1 - '$MEAN')^2} END {printf "%f", sqrt(sum/'$N')}' $1`
	ERROR=`awk 'BEGIN {printf "%f", '$Z' * '$STDEV'/sqrt('$N')}'`
}

process() {
	local N=`wc -l $1 | cut -d' ' -f1`
	echo $N > .tmp
	cat $1 >> .tmp
	echo -ne "$3\t$4\t$N\n" >> $2/n_lines.txt
	./percentile ${PERCENTILE_1} .tmp >> $2/percentiles_${PERCENTILE_1}.txt
	./percentile ${PERCENTILE_2} .tmp >> $2/percentiles_${PERCENTILE_2}.txt
}

echo 3 | sudo tee /proc/sys/vm/drop_caches 1>/dev/null 2>/dev/null

#ssh ${SERVER_IP} "echo 0 | sudo tee /proc/sys/kernel/nmi_watchdog" 1>/dev/null 2>/dev/null

for l in ${SERVER_LAYOUT_LIST[@]}; do

	if [ $l = "l4" ]; then
		SERVER_NUMBER_OF_CORES="2 6"
	else
		SERVER_NUMBER_OF_CORES="8"
	fi

	STOP=0
	CLIENT_CURRENT_RATE=${CLIENT_RATE_INITIAL}

	while [ ${STOP} -eq 0 ]; do
		CLIENT_CURRENT_RATE=$(( CLIENT_CURRENT_RATE + CLIENT_RATE_INCREMENTAL ))
		DIR="results/$l/${CLIENT_CURRENT_RATE}"
		rm -rf $DIR 
		mkdir -p $DIR 
		rm -rf ${CLIENT_OUTPUT_FILE}

		for j in `seq 0 $RUNS`; do
			echo "Layout: $l -- Run: $j/$RUNS -- Rate: ${CLIENT_CURRENT_RATE}"

			## Run the server
			SERVER_SCRIPT_ARGS="${SERVER_NUMBER_OF_CORES} ${SERVER_APPLICATION}"
			ssh ${SERVER_IP} "cd $l/demikernel; sh ./run_server.sh '${SERVER_CORES}' '${SERVER_SCRIPT_ARGS}'" 1>/dev/null 2>/dev/null &

			## Sleep a while
			sleep 2

			## Run the client
			SEED=${SERVER_SEED_LIST[j]}
			sudo LD_LIBRARY_PATH=${HOME}/lib/x86_64-linux-gnu timeout ${SERVER_TIMEOUT} ./build/tcp-generator -a ${CLIENT_PCI_NIC} -n 4 -l ${CLIENT_CORES} -- -d ${CLIENT_INTERARRIVAL} -r ${CLIENT_CURRENT_RATE} -f ${CLIENT_FLOWS} -s ${CLIENT_SIZE} -t ${CLIENT_DURATION} -q ${CLIENT_QUEUES} -e ${SEED} -c ${CLIENT_CONF_FILE} -o ${CLIENT_OUTPUT_FILE} -D ${SERVER_APPLICATION_DISTRIBUTION} -i ${SERVER_APPLICATION_ITERATIONS_1} -j ${SERVER_APPLICATION_ITERATIONS_2} 1>/dev/null 2>/dev/null &

			## Sleep a while
			sleep ${SERVER_TIMEOUT}
			sleep 10

			## Process the output file
			if [ ! -f ${CLIENT_OUTPUT_FILE} ]; then
				STOP=1
				break
			fi

			REAL_N=`wc -l ${CLIENT_OUTPUT_FILE} | cut -d' ' -f1`
			EXPECTATIVE_N=$(( (CLIENT_CURRENT_RATE * CLI_DURATION * 99) / 100 ))

			if [[ ${REAL_N} -le ${EXPECTATIVE_N} ]]; then
				STOP=1
				break
			fi

			ssh ${SERVER_IP} "sudo pkill -9 perf" 1>/dev/null 2>/dev/null
			scp ${SERVER_IP}:~/$l/demikernel/output.perf output$j.perf 1>/dev/null 2>/dev/null
			mv output$j.perf $DIR 1>/dev/null 2>/dev/null

			process ${CLIENT_OUTPUT_FILE} $DIR ${CLIENT_CURRENT_RATE} $j
		done
	done
done

rm -rf .tmp 1>/dev/null 2>/dev/null
rm -rf ${CLIENT_OUTPUT_FILE} 1>/dev/null 2>/dev/null
#ssh ${SERVER_IP} "echo 1 | sudo tee /proc/sys/kernel/nmi_watchdog" 1>/dev/null 2>/dev/null
