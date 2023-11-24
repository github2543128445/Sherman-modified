# Yahoo! Cloud System Benchmark
# Workload C: Read only
#   Application example: user profile cache, where profiles are constructed elsewhere (e.g., Hadoop)
#                        
#   Read/update ratio: 100/0
#   Default data size: 1 KB records (10 fields, 100 bytes each, plus key)
#   Request distribution: zipfian

recordcount=800000000
operationcount=50000000
workload=com.yahoo.ycsb.workloads.CoreWorkload

readallfields=true
writeallfields=true
zeropadding=20

fieldcount=0
fieldlength=0

readproportion=1
updateproportion=0
scanproportion=0
insertproportion=0

requestdistribution=zipfian



