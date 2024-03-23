#!/bin/bash

create_ccdf() {
    NMAX=`wc -l $1 | cut -d' ' -f1`
    MAX=$(( NMAX - 1 ))

    cat $1 | cut -d$'\t' -f1 | sort -n > .sorted
    cat .sorted | uniq --count | awk -v MAX="$MAX" 'BEGIN { sum = 0 } { print $2, $1, (sum/MAX); sum = sum + $1 }' | sed 's/,/./g' > output.ccdf
    rm .sorted 1>/dev/null 2>/dev/null
}

create_ccdf $1