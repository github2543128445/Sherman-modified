

#/bin/bash

trap 'kill $(jobs -p)' SIGINT

workloads="./workloads/workloadc.spec ./workloads/write.spec ./workloads/update.spec ./workloads/scan.spec \
./workloads/workloada.spec ./workloads/workloadb.spec ./workloads/workloadd.spec ./workloads/workloade.spec ./workloads/workloadf.spec"

threads="36"

for thread in $threads; do
    for file_name in $workloads; do
        sleep 5 
        echo "Running Redis with for $file_name thread $thread"
        ssh kvgroup@skv-node1 "sleep 10; cd ./wgl/Sherman-CX5; ./run_client.sh; exit;"
        sleep 5 
        ssh kvgroup@skv-node2 "sleep 10; cd ./wgl/Sherman-CX5; ./run_client.sh; exit;"
        sleep 5 
        ssh kvgroup@skv-node4 "sleep 10; cd ./wgl/Sherman-CX5; ./run_client.sh; exit;"
        sleep 5 
        nohup ./ycsbc -db sherman -threads $thread -P $file_name >./data/exp0_sherman_$file_name.txt 2>&1 &
        echo "node 6"
        sleep 1
        ssh kvgroup@skv-node5 "cd ./wgl/YCSB-C; nohup ./ycsbc -db sherman -threads $thread -P $file_name >./data/exp0_sherman_$file_name.txt 2>&1 &; exit;"
        echo "node 5"
        sleep 1
        ssh kvgroup@skv-node7 "cd ./wgl/YCSB-C; ./ycsbc -db sherman -threads $thread -P $file_name >./data/exp0_sherman_$file_name.txt 2>&1; exit;"
        echo "node 7"
        sleep 40
        ssh kvgroup@skv-node1 "sleep 10; cd ./wgl/Sherman-CX5; ./kill_client.sh; exit;"
        echo "node 1 finish"
        sleep 5
        ssh kvgroup@skv-node2 "sleep 10; cd ./wgl/Sherman-CX5; ./kill_client.sh; exit;"
        echo "node 2 finish"
        sleep 5
        ssh kvgroup@skv-node4 "sleep 10; cd ./wgl/Sherman-CX5; ./kill_client.sh; exit;"
        echo "node 3 finish"
        sleep 5
        echo "Finish $file_name"
        wait
    done
done

echo "Finished"