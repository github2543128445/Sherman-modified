

#/bin/bash
workloads="../workloads/workloadc.spec ../workloads/scan.spec"
threads="36"

for thread in $threads; do
    for file_name in $workloads; do
        sleep 10
        echo "Running Redis with for $file_name thread $thread"
        ssh kvgroup@skv-node5 "sleep 10; cd ./wgl/Sherman-CXT5/build; ./run_client.sh; exit;"
        sleep 10
        ./ycsbc -db sherman -threads $thread -P $file_name
        sleep 10
        ssh kvgroup@skv-node5 "sleep 10; cd ./wgl/Sherman-CXT5/build; ./kill_client.sh; sleep 10; exit;"
        sleep 10
        wait
    done
done

echo "Finished"
