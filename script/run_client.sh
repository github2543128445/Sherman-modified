#/bin/bash

./restart.sh;
sleep 30;
`nohup ./benchmark 2 100 1 >> log.out 2>&1 & echo $! > sherman_log.out`; echo "server"; 
sleep 100;