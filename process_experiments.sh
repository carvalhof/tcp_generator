#!/bin/bash

PERCENTILE_1="50.0"
PERCENTILE_2="99.9"

SERVER_LAYOUT_LIST=("l1" "l2" "l2+" "l3" "l4" "l4+")
CLIENT_RATE_INITIAL=10000
CLIENT_RATE_INCREMENTAL=10000

RESDIR="results-sqrt-exp-50us"

error () {
    local Z=1.96
    local N=`wc -l $1 | cut -d' ' -f1`

    MEAN=`awk '{sum += $1} END {printf "%f", (sum/NR)}' $1`
    STDEV=`awk '{sum += ($1 - '$MEAN')^2} END {printf "%f", sqrt(sum/'$N')}' $1`
    ERROR=`awk 'BEGIN {printf "%f", '$Z' * '$STDEV'/sqrt('$N')}'`
}

percentile () {
    local N=`wc -l $1 | cut -d' ' -f1`
    echo $N > .tmp
    cat $1 | cut -d$'\t' -f1 >> .tmp
    ./percentile $2 .tmp >> $3/percentiles_$2.txt
}

for l in ${SERVER_LAYOUT_LIST[@]}; do
    CLIENT_CURRENT_RATE=$(( CLIENT_RATE_INITIAL + CLIENT_RATE_INCREMENTAL ))
    DIR="$RESDIR/$l/${CLIENT_CURRENT_RATE}"

    OUTPUT_FILE="$RESDIR/$l.dat"
    rm -rf $OUTPUT_FILE 1>/dev/null 2>/dev/null

    while [ -d $DIR ]; do
        INPUT="$DIR/output0.dat"

        if [ ! -f $INPUT ]; then
            break;
        fi

        percentile $INPUT ${PERCENTILE_1} $DIR
        error "$DIR/percentiles_${PERCENTILE_1}.txt"
        echo -ne "$CLIENT_CURRENT_RATE\t$MEAN\t$ERROR\t" >> $OUTPUT_FILE
        percentile $INPUT ${PERCENTILE_2} $DIR
        error "$DIR/percentiles_${PERCENTILE_2}.txt"
        echo -ne "$MEAN\t$ERROR\n" >> $OUTPUT_FILE

        CLIENT_CURRENT_RATE=$(( CLIENT_CURRENT_RATE + CLIENT_RATE_INCREMENTAL  ))
        DIR="$RESDIR/$l/${CLIENT_CURRENT_RATE}"
    done
done
