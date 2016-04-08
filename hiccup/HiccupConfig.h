#ifndef _HICCUPCONFIG_H_
#define _HICCUPCONFIG_H_

struct HiccupConfig {
  int runtime_;         // Total run time, in seconds.
  int resolution_;      // nano-seconds per bin, 200ns, 1/5us.
  int ccperbin_;
  int verbose_;         // report historgrams
  int priority_;        // RT priority
  int policy_;          // OTHER, FIFO, Round-Robin
  int bins_;                // number of bins used by each thread.
  int maxcpus_;
  int maxdelay_us_;     // all delays longer then this are in the last bin.
  int cpus_[MaxCpuCores];   // test run on these cpu cores.

  HiccupConfig () :
    runtime_(5),        // Default runtime of 5 seconds
    resolution_(200),       // Default histogram resolution of 200 nano seconds
    ccperbin_(0),       // CpuCycles (timestamp counter) per histogram bin
    verbose_(0),
    priority_(0),       // Priority
    policy_(SCHED_OTHER),   // Scheduler policy
    bins_(-1),          // calc at init, before allocating the bins: MaxHiccup * CCperUS / ccperbin_
    maxcpus_(0),
    maxdelay_us_(200)       // all delay longer then 200 mico-seconds are collected to the last bin
  { }
  
  void init() {
    bins_ = maxdelay_us_ * syscpus.ccpermicro_ / ( resolution_ * syscpus.ccpermicro_ / 1000 ) + 1;
    ccperbin_ = resolution_ * syscpus.ccpermicro_ / 1000 + 1;
  }
  void print() {
    fprintf(stdout, "Hiccups Configuration:\n"
        "  Run Time: %d [s]\n"
        "  Histogram resolution: %d [ns]\n"
        "  RT Priority: %d\n"
        "  bins: %d\n"
        "  cpus(%d):",
        runtime_, resolution_, priority_, bins_, maxcpus_);
    for (int ii = 0; ii < maxcpus_; ii++)
      fprintf(stdout, " %d", cpus_[ii]);
    fprintf(stdout, "\n");
  }
};

#endif