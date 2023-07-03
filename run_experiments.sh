#!/bin/bash

RUNS=4
PERCENTILE_1="50.0"
PERCENTILE_2="99.9"

SERVER_TIMEOUT=35
SERVER_IP=130.127.134.63
SERVER_LAYOUT_LIST=("l1" "l2" "l3" "l4")
SERVER_SEED_LIST=(1646203793 986508091 193720917 15335381 633882127 1093215650 772188468 711307909 645856549 1127581467 765061083 1050115427 4231379 1000215989 1382853168 1927405477 306097907 1344972625 2098183364 323989894)
SERVER_APPLICATION="null"
SERVER_CORES="1,3,5,7,9,11,13,15,17,19,21,23,25,27,29"

CLIENT_DURATION=10
CLIENT_PCI_NIC="ca:00.0"
CLIENT_CORES="3,5,7,9"
CLIENT_FLOWS=128
CLIENT_SIZE=128
CLIENT_CONF_FILE="addr.cfg"
CLIENT_OUTPUT_FILE="output.dat"
CLIENT_CSV_FILE="csv.csv"

TIMEOUT=$(( SERVER_TIMEOUT - CLIENT_DURATION - CLIENT_DURATION ))

# offset   0  -- low load	(00-10)
# offset 750  -- medium load 	(10-20)
# offset 1285 -- high load 	(20-30)
# offset 6620 -- super highload (30-90)

CLIENT_OFFSET_RANGE=3
CLIENT_OFFSET_VALUES=(0 750 1285 6620)
CLIENT_OFFSET_NAMES=("low" "medium" "high" "super_high")

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

	for i in `seq 0 ${CLIENT_OFFSET_RANGE}`; do
		NAME=${CLIENT_OFFSET_NAMES[i]}
		VALUE=${CLIENT_OFFSET_VALUES[i]}

		DIR="results/$l/${NAME}"
		rm -rf $DIR 
		mkdir -p $DIR 
		rm -rf ${CLIENT_OUTPUT_FILE}

		for j in `seq 0 $RUNS`; do
			echo "Layout: $l -- Run: $j/$RUNS -- Load: ${NAME}"

			## Run the server
			SERVER_SCRIPT_ARGS="${SERVER_NUMBER_OF_CORES} ${SERVER_APPLICATION}"
			ssh ${SERVER_IP} "cd $l/demikernel; sh ./run_server.sh '${SERVER_CORES}' '${SERVER_SCRIPT_ARGS}'" 1>/dev/null 2>/dev/null &

			## Sleep a while
			sleep 2

			## Run the client
			SEED=${SERVER_SEED_LIST[j]}
			sudo LD_LIBRARY_PATH=${HOME}/lib/x86_64-linux-gnu timeout ${SERVER_TIMEOUT} ./build/tcp-generator -a ${CLIENT_PCI_NIC} -n 4 -l ${CLIENT_CORES} -- -f ${CLIENT_FLOWS} -s ${CLIENT_SIZE} -t ${CLIENT_DURATION} -e ${SEED} -c ${CLIENT_CONF_FILE} -o ${CLIENT_OUTPUT_FILE} -C ${CLIENT_CSV_FILE} -i ${VALUE} 1>/dev/null 2>/dev/null &

			## Sleep a while
			sleep ${SERVER_TIMEOUT}
			sleep 5

			## Process the output file
			if [ ! -f ${CLIENT_OUTPUT_FILE} ]; then
				break
			fi

			ssh ${SERVER_IP} "sudo pkill -9 perf" 1>/dev/null 2>/dev/null
			scp ${SERVER_IP}:~/$l/demikernel/output.perf output$j.perf 1>/dev/null 2>/dev/null
			mv output$j.perf $DIR 1>/dev/null 2>/dev/null

			process ${CLIENT_OUTPUT_FILE} $DIR ${NAME} $j
		done
	done
done

rm -rf .tmp 1>/dev/null 2>/dev/null
#rm -rf ${CLIENT_OUTPUT_FILE} 1>/dev/null 2>/dev/null
#ssh ${SERVER_IP} "echo 1 | sudo tee /proc/sys/kernel/nmi_watchdog" 1>/dev/null 2>/dev/null
