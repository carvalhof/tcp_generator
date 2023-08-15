# TCP_generator

Follow these instructions to build the tcp generator using DPDK 22.11 and CloudLab nodes

## Building

> **Make sure that `PKG_CONFIG_PATH` is configured properly.**

```bash
git clone https://github.com/carvalhof/tcp_generator
cd tcp_generator
PKG_CONFIG_PATH=$HOME/lib/x86_64-linux-gnu/pkgconfig make
```

## Running

> **Make sure that `LD_LIBRARY_PATH` is configured properly.**

```bash
sudo LD_LIBRARY_PATH=$HOME/lib/x86_64-linux-gnu ./build/tcp-generator -a 41:00.0 -n 4 -c 0xff -- -f $FLOWS -s $SIZE -t $DURATION -e $SEED -C $CSV_FILE -i $INDEX_OF_CSV -c $ADDR_FILE -o $OUTPUT_FILE -a $APPLICATION
```

> **Example**

```bash
sudo LD_LIBRARY_PATH=$HOME/lib/x86_64-linux-gnu ./build/tcp-generator -a 41:00.0 -n 4 -c 0xff -- -f 1 -s 128 -t 10 -e 37 -C csv.csv -i 0 -c addr.cfg -o output.dat -a sqrt
```

### Parameters

- `$FLOWS` : number of flows
- `$SIZE` : packet size in _bytes_
- `$DURATION` : duration of execution in _seconds_ (we double for warming up)
- `$SEED` : seed number
- `$ADDR_FILE` : name of address file (_e.g.,_ 'addr.cfg')
- `$CSV_FILE` : name of CSV file
- `$INDEX_OF_CSV_FILE` : index of the CSV file
- `$OUTPUT_FILE` : name of output file containg the latency for each packet
- `$APPLICATION` : application (sqrt/stridedmem/null)


### _addresses file_ structure

```
[ethernet]
src = 0c:42:a1:8c:db:1c
dst = 0c:42:a1:8c:dc:54

[ipv4]
src = 192.168.1.2
dst = 192.168.1.1

[tcp]
dst = 12345

[application]
sqrt = 1.97
stridedmem = 1.33
null = 1
```
