#!/bin/bash

PERCENTILE_1="50.0"
PERCENTILE_2="99.9"

SERVER_LAYOUT_RANGE=3
SERVER_LAYOUT_LIST=("l1" "l2" "l3" "l4")
CLIENT_OFFSET_RANGE=2
CLIENT_OFFSET_VALUES=(0 750 1285 6620)
CLIENT_OFFSET_NAMES=("low" "medium" "high" "super_high")

LAYOUT_NAMES=("Layout 1" "Layout 2" "Layout 3" "Layout 4")
LAYOUT_DESCRIPTIONS=("cFCFS + Same Core" "dFCFS + Same Core" "cFCFS + Dif. Cores" "dFCFS + Dif. Cores")
error () {
    local Z=1.96
    local N=`wc -l $1 | cut -d' ' -f1`

    MEAN=`awk '{sum += $1} END {printf "%f", (sum/NR)}' $1`
    STDEV=`awk '{sum += ($1 - '$MEAN')^2} END {printf "%f", sqrt(sum/'$N')}' $1`
    ERROR=`awk 'BEGIN {printf "%f", '$Z' * '$STDEV'/sqrt('$N')}'`
}

OUTPUT_FILE="results.dat"
rm -rf $OUTPUT_FILE 1>/dev/null 2>/dev/null
for i in `seq 0 ${SERVER_LAYOUT_RANGE}`; do
    l=${SERVER_LAYOUT_LIST[i]}
    echo -ne "${LAYOUT_NAMES[i]} (${LAYOUT_DESCRIPTIONS[i]})\n" >> ${OUTPUT_FILE}
    for j in `seq 0 ${CLIENT_OFFSET_RANGE}`; do
	NAME=${CLIENT_OFFSET_NAMES[j]}
	VALUE=${CLIENT_OFFSET_VALUES[j]}
	DIR="results/$l/${NAME}"
        FILE1="$DIR/percentiles_${PERCENTILE_1}.txt"
        FILE2="$DIR/percentiles_${PERCENTILE_2}.txt"

        if [ ! -f $FILE1 ]; then
            continue
        fi
        error $FILE1
        echo -ne "\t$NAME\t$MEAN\t$ERROR\t" >> ${OUTPUT_FILE}
        error $FILE2
        echo -ne "$MEAN\t$ERROR\n" >> ${OUTPUT_FILE}
    done
    echo -ne "#---------------------------------\n" >> ${OUTPUT_FILE}
done
