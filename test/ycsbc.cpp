//
//  ycsbc.cc
//  YCSB-C
//
//  Created by Jinglei Ren on 12/19/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#include <cstring>
#include <string>
#include <iostream>
#include <vector>
#include <future>
#include "core/utils.h"
#include "core/timer.h"
#include "core/core_workload.h"
// #include <gperftools/profiler.h>
#include <unistd.h>
#include "Timer.h"
#include "Tree.h"

#include <city.h>
#include <stdlib.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>

// #define USE_CORO
#define SHERMAN_LATENCY
#define SHERMAN_MAX_LATENCY_SIZE 100000

double warm_ratio = 0.8;
int kCoroCnt = 2;
using namespace std;

Tree *tree;
DSM *dsm;
ycsbc::CoreWorkload wl;
ycsbc::CoreWorkload warm_wl;
atomic<uint64_t> load_count;

#ifdef SHERMAN_LATENCY
uint64_t exeTime[MAX_APP_THREAD];
struct timespec time_start[MAX_APP_THREAD];
struct timespec time_end[MAX_APP_THREAD];

double request_latency[MAX_APP_THREAD];
atomic_int request_count[MAX_APP_THREAD];
uint64_t dis_latency[MAX_APP_THREAD][SHERMAN_MAX_LATENCY_SIZE];

#endif

void reset() {
    cout << "A part finished." << endl;
#ifdef SHERMAN_LATENCY
    for(int i = 0; i < MAX_APP_THREAD; i++) {
        request_count[i].store(0);
        request_latency[i] = 0;
        memset(dis_latency[i], 0, sizeof(dis_latency[i]));
    }
#endif
}

void bindcore(uint16_t core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
    }
}

Key str2key(string key) {
    Key t_key;
    memcpy(t_key.key, key.c_str(), sizeof(t_key));
    return t_key;
}

int TransactionRead() {
    const std::string &key = wl.NextTransactionKey();
    Value value;
    tree->search(str2key(key), value);
    return 0;
}

int TransactionReadModifyWrite() {
    const std::string &key = wl.NextTransactionKey();
    Value value;
    tree->search(str2key(key), value);
    tree->insert(str2key(key), value);
    return 0;
}

int TransactionScan() {
    std::string from;
    std::string to;
    int len = wl.NextScanLength();
    if(len == 0) {
        len = 1;
    }
    wl.NextTransactionKeyRange(len, from, to);
    Value values[1000];
    tree->range_query(str2key(from), str2key(to), len, values);
    return 0;
}

 int TransactionUpdate() {
    const std::string &key = wl.NextTransactionKey();
    Value value = 12;
    tree->insert(str2key(key), value);
    return 0;
}

int TransactionInsert() {
    const std::string &key = wl.NextSequenceKey();
    Value value = 12;
    tree->insert(str2key(key), value);
    return 0;
}

void do_transaction() {
#ifdef SHERMAN_LATENCY
    int th_id = dsm->getMyThreadID();
    request_count[th_id]++;
    clock_gettime(CLOCK_REALTIME, &time_start[th_id]);
#endif
    bool status;
    switch (wl.NextOperation()) {
        case ycsbc::READ:
            status = TransactionRead();
        break;
        case ycsbc::UPDATE:
            status = TransactionUpdate();
        break;
        case ycsbc::INSERT:
            status = TransactionInsert();
        break;
        case ycsbc::SCAN:
            status = TransactionScan();
        break;
        case ycsbc::READMODIFYWRITE:
            status = TransactionReadModifyWrite();
        break;
        default:
        throw utils::Exception("Operation request is not recognized!");
    }
#ifdef SHERMAN_LATENCY
    clock_gettime(CLOCK_REALTIME, &time_end[th_id]);
    exeTime[th_id] = (time_end[th_id].tv_sec - time_start[th_id].tv_sec) 
            * 1000000000 + 
            (time_end[th_id].tv_nsec - time_start[th_id].tv_nsec);
    request_latency[th_id] += exeTime[th_id];
    dis_latency[th_id][exeTime[th_id]/100]++;
    if(request_count[th_id] % 100000 == 0) {
        printf("_____________________________\n");
        printf("request %d times, request average time %lf ns\n", 
                request_count[th_id].load(),  request_latency[th_id] / (double)(request_count[th_id].load()));
        int lat_count = 0;
        for(int i = 0; i < SHERMAN_MAX_LATENCY_SIZE; i++) {
            lat_count += dis_latency[th_id][i];
            if(lat_count > 0.5 * (double)(request_count[th_id].load())) {
                cout<<"thread "<<th_id<<" P50 latency "<<i*100<<" ns"<<endl;
                break;
            }
        }
        lat_count = 0;
        for(int i = 0; i < SHERMAN_MAX_LATENCY_SIZE; i++) {
            lat_count += dis_latency[th_id][i];
            if(lat_count > 0.99 * (double)(request_count[th_id].load())) {
                cout<<"thread "<<th_id<<" P99 latency "<<i*100<<" ns"<<endl;
                break;
            }
        }
        tree->index_cache_statistics();
    }
#endif
}

int thread_load(const int num_ops, int id) {
    bindcore(id);
    dsm->registerThread();
    int count = 0;
    for (int i = 0; i < num_ops; ++i) {
        load_count++;
        if(load_count % 100000 == 0) {
            printf("load %d times\n", load_count.load());
        }

        TransactionInsert();
        count++;
    }
    return count;
}

int thread_warm(const int num_ops, int id) {
    bindcore(id);
    dsm->registerThread();
    int count = 0;
    for (int i = 0; i < num_ops; ++i) {
        const std::string &key = warm_wl.NextSequenceKey();
        Value value = 12;
        tree->search(str2key(key), value);
        count++;
    }
    return count;
}

int thread_run(const int num_ops, const int total_ops, int id) {
    bindcore(id);
    dsm->registerThread();
    int count = 0;
#ifdef USE_CORO
  tree->run_coroutine(&wl, id, total_ops, kCoroCnt);
  count += num_ops;
#else
    for (int i = 0; i < num_ops; ++i) {
        do_transaction();
        count++;
    }
#endif  
    assert(count == num_ops);
    return count;
}

void UsageMessage(const char *command) {
  cout << "Usage: " << command << " [options]" << endl;
  cout << "Options:" << endl;
  cout << "  -threads n: execute using n threads (default: 1)" << endl;
  cout << "  -db dbname: specify the name of the DB to use (default: basic)" << endl;
  cout << "  -P propertyfile: load properties from the given file. Multiple files can" << endl;
  cout << "                   be specified, and will be processed in the order specified" << endl;
}

bool StrStartWith(const char *str, const char *pre) {
  return strncmp(str, pre, strlen(pre)) == 0;
}

string ParseCommandLine(int argc, const char *argv[], utils::Properties &props) {
  int argindex = 1;
  string filename;
  while (argindex < argc && StrStartWith(argv[argindex], "-")) {
    if (strcmp(argv[argindex], "-threads") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("threadcount", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-db") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("dbname", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-host") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("host", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-port") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("port", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-slaves") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      props.SetProperty("slaves", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-P") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        exit(0);
      }
      filename.assign(argv[argindex]);
      ifstream input(argv[argindex]);
      try {
        props.Load(input);
      } catch (const string &message) {
        cout << message << endl;
        exit(0);
      }
      input.close();
      argindex++;
    } else {
      cout << "Unknown option '" << argv[argindex] << "'" << endl;
      exit(0);
    }
  }

  if (argindex == 1 || argindex != argc) {
    UsageMessage(argv[0]);
    exit(0);
  }

  return filename;
}

int main(const int argc, const char *argv[]) {

    load_count.store(0);
    // kNodeCount = atoi(argv[1]);
    // kReadRatio = atoi(argv[2]);
    // kThreadCount = atoi(argv[3]);
    
    // parameters
    int kNodeCount = 2;
    atomic<uint64_t> th_id;

    DSMConfig config;
    config.machineNR = kNodeCount;
    dsm = DSM::getInstance(config);

    dsm->registerThread();
    tree = new Tree(dsm);
    cout << "Init 1" << endl;

    dsm->barrier("benchmark");
    dsm->set_barrier("running");
    cout << "Init finish begins working." << endl;

    // loading
    cout << "Loading" << endl;

    utils::Properties props;
    string file_name = ParseCommandLine(argc, argv, props);
    const int num_threads = stoi(props.GetProperty("threadcount", "1"));

    wl.Init(props);
    // Loads data
    th_id.store(0);
    dsm->resetThread();
    vector<future<int>> actual_ops;
    int total_ops = stoi(props[ycsbc::CoreWorkload::RECORD_COUNT_PROPERTY]);
    for (int i = 0; i < 20; ++i) {
        sleep(10);
        th_id++;
        actual_ops.emplace_back(async(launch::async,
            thread_load, total_ops / 20, th_id.load()));
    }
    assert((int)actual_ops.size() == 20);

    int sum = 0;
    for (auto &n : actual_ops) {
        assert(n.valid());
        sum += n.get();
    }
    cerr << "# Loading records:\t" << sum << endl;

    // Warm data
    th_id.store(0);
    dsm->resetThread();
    warm_wl.Init(props);
    actual_ops.clear();
    for (int i = 0; i < 50; ++i) {
        th_id++;
        actual_ops.emplace_back(async(launch::async,
            thread_warm, (total_ops * warm_ratio) / 50, th_id.load()));
    }
    assert((int)actual_ops.size() == 50);

    sum = 0;
    for (auto &n : actual_ops) {
        assert(n.valid());
        sum += n.get();
    }
    cerr << "# Warm records:\t" << sum << endl;
    reset();
    tree->clear_statistics();

    int cs_num = 3;
    dsm->barrier("running", cs_num);
    // Peforms transactions
    th_id.store(0);
    dsm->resetThread();
    // wl.SetStart(stoi(props[ycsbc::CoreWorkload::RECORD_COUNT_PROPERTY]));
    actual_ops.clear();
    total_ops = stoi(props[ycsbc::CoreWorkload::OPERATION_COUNT_PROPERTY]);
    utils::Timer<double> timer;
    timer.Start();
    for (int i = 0; i < num_threads; ++i) {
        th_id++;
        actual_ops.emplace_back(async(launch::async,
            thread_run, total_ops / num_threads, total_ops, th_id.load()));
    }
    assert((int)actual_ops.size() == num_threads);
    
    sum = 0;
    for (auto &n : actual_ops) {
        assert(n.valid());
        sum += n.get();
    }
    double duration = timer.End();
    cerr << "# Transaction throughput (KTPS)" << endl;
    cerr << props["dbname"] << '\t' << file_name << '\t' << num_threads << '\t';
    cerr << total_ops / duration / 1000 << endl;

    return 0;
}