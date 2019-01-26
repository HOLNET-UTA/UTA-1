#!/bin/bash 
stopTime="10s"
for load in $(seq 30 10 80)
do	
	for s in $(seq 1 1 4)
	do
	{
		sudo ./waf --run "scratch/dcmgr-test-myfifo --useModel=2 --rcos=3 --numSpines=6 --numLeafs=6 --numHostsPerLeaf=24 --seed=${s} --load=${load} --trafficType=1 --flowStopTime=${stopTime} --fabricQueueSize=300 edgeQueueSize=150 --MediumOffset=0.001 --MediumSpeed=1 --BigOffset=0 --BigSpeed=1.5" > dcmrg_${load}_${s}_dm_log.out 2>&1 &	
	}
	done
done

