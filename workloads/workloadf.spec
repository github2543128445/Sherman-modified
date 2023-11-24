# Yahoo! Cloud System Benchmark
# Workload A: Update heavy workload
#   Application example: Session store recording recent actions
#                        
#   Read/update ratio: 50/50
#   Default data size: 1 KB records (10 fields, 100 bytes each, plus key)
#   Request distribution: zipfian

recordcount=200000000
operationcount=100000000
workload=com.yahoo.ycsb.workloads.CoreWorkload

readallfields=true
writeallfields=true
zeropadding=20

fieldcount=0
fieldlength=0

readproportion=0.5
updateproportion=0
scanproportion=0
insertproportion=0
readmodifywriteproportion=0.5

requestdistribution=zipfian


