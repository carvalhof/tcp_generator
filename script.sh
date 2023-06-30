#!/bin/bash

## To compile
PKG_CONFIG_PATH=$HOME/lib/x86_64-linux-gnu/pkgconfig make
## To execute
sudo LD_LIBRARY_PATH=$HOME/lib/x86_64-linux-gnu ./build/tcp-generator -a ca:00.0 -n 4 -l 1,3,5,7,9 -- -d exponential -r 100000 -f 128 -s 128 -t 10 -q 1 -e 37 -c addr.cfg -o output.dat -D constant -i 0
