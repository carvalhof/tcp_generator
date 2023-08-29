#!/bin/bash

N=`cat /proc/cpuinfo | grep MHz | wc -l`

for i in `seq 0 $(( N - 1 ))`; do
        sudo cpufreq-set -r -c $i -g performance 1>/dev/null 2>/dev/null
done

echo 16384 | sudo tee /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
echo 16384 | sudo tee /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
sudo mkdir -p /mnt/huge 1>/dev/null 2>/dev/null
mount -t hugetlbfs -opagesize=2M nodev /mnt/huge

sudo setpci -s d8:00.0 68.w=393e
sudo setpci -s d8:00.1 68.w=393e

sudo sysctl -w net.ipv6.conf.ens3f0np0.disable_ipv6=1
sudo sysctl -w net.ipv6.conf.ens3f1np1.disable_ipv6=1
sudo ethtool -A ens3f0np0 rx off tx off
sudo ethtool -A ens3f1np1 rx off tx off
systemctl stop irqbalance

sudo sysctl -w vm.zone_reclaim_mode=0
sudo sysctl -w vm.swappiness=0
sudo swapoff -a
sudo sysctl vm.stat_interval=120
echo 0 | sudo tee /sys/kernel/mm/ksm/run
echo 0 | sudo tee /proc/sys/kernel/numa_balancing
echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
find /sys/devices/virtual/workqueue -name cpumask  -exec sh -c 'echo 1 | sudo tee {}' ';'
echo -1 > /proc/sys/kernel/sched_rt_runtime_us

./turbo.sh disable
