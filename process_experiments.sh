#!/bin/bash

PERCENTILE_1="50.0"
PERCENTILE_2="99.9"

SERVER_LAYOUT_LIST=("l1" "l2" "l3" "l4")
CLIENT_RATE_INITIAL=50000
CLIENT_RATE_INCREMENTAL=50000

error () {
    local Z=1.96
    local N=`wc -l $1 | cut -d' ' -f1`

    MEAN=`awk '{sum += $1} END {printf "%f", (sum/NR)}' $1`
    STDEV=`awk '{sum += ($1 - '$MEAN')^2} END {printf "%f", sqrt(sum/'$N')}' $1`
    ERROR=`awk 'BEGIN {printf "%f", '$Z' * '$STDEV'/sqrt('$N')}'`
}

for l in ${SERVER_LAYOUT_LIST[@]}; do
    CLIENT_CURRENT_RATE=$(( CLIENT_RATE_INITIAL + CLIENT_RATE_INCREMENTAL ))
    DIR="results/$l/${CLIENT_CURRENT_RATE}"

    OUTPUT_FILE="results/$l.dat"
    rm -rf $OUTPUT_FILE 1>/dev/null 2>/dev/null

    while [ -d $DIR ]; do
        FILE1="$DIR/percentiles_${PERCENTILE_1}.txt"
        FILE2="$DIR/percentiles_${PERCENTILE_2}.txt"

        if [ ! -f $FILE1 ]; then
            break;
        fi

        error $FILE1
        echo -ne "$CLIENT_CURRENT_RATE\t$MEAN\t$ERROR\t" >> $OUTPUT_FILE
        error $FILE2
        echo -ne "$MEAN\t$ERROR\n" >> $OUTPUT_FILE

	CLIENT_CURRENT_RATE=$(( CLIENT_CURRENT_RATE + CLIENT_RATE_INCREMENTAL  ))
    DIR="results/$l/${CLIENT_CURRENT_RATE}"
    done
done
