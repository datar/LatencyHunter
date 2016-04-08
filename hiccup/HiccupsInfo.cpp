#include "HiccupsInfo.h"

void* HiccupsInfo::runthread(void* vp) {
  ThrdStart* ts = (ThrdStart*)vp;
  int id = ts->thrdid_;
  int cpu;

  runoncpu(conf.cpus_[id]);
  if ((cpu = sched_getcpu()) < 0) {
    perror("sched_getcpu");
    exit(1);
  }
  if (cpu != conf.cpus_[id]) {
    fprintf(stderr, "Thread %d: tid: %d, running on wrong cpu: %d, expected: %d\n",
        id, gettid(), cpu, conf.cpus_[id]);
    exit(1);
  }
  unsigned long sz = HPGSZ(sizeof(HiccupsInfo));

  fprintf(stdout, "thread#: %3d tid: %d size %lu cpu[%d]: %d\n",
      id, gettid(), sz, id, conf.cpus_[id]);
  HiccupsInfo::hudatap_[id] = ts->hudata_ = (HiccupsInfo*)memzalloc(sz);
  ts->hudata_->cpuid_ = conf.cpus_[id];
  ts->hudata_->id_ = id;
  ts->hudata_->bins_ = (HUbin*)memzalloc(conf.bins_ * sizeof(bins_[0]));

  ts->rv_ = ts->hudata_->run();
  if (id)
    pthread_exit(0);
  return 0;
}

uint64_t HiccupsInfo::run() {
  uint64_t cc, lcc, ccend, dt, tsum = 0, ccstart;
  int b;

  stackreserve();
  for (int ii = 0; ii < conf.bins_; ii++ )
    bins_[ii].n_ = bins_[ii].sum_ = 0;
  if (conf.priority_) {
    sched_param sp;
    memset(&sp, 0, sizeof (sp));
    sp.sched_priority = conf.priority_;
    if (sched_setscheduler(gettid(), conf.policy_, &sp))
      perror("set scheduler");
  }
  if (id_) {
    __sync_add_and_fetch(&active_, 1);
    while (runstate_ != RSRUN)
      ;
  }
  else {
    while (1) {
      int z = active_;
      __sync_synchronize();
      if (z >= conf.maxcpus_ - 1)
        break;
      usleep(2000);
    }
    runstate_ = RSRUN;
  }
  ccend = (lcc = cc = ccstart = startcc_ = rdtsc()) + conf.runtime_ * syscpus.ccpersec_;
  if (id_)
    ccend += 500 * syscpus.ccpermicro_;
  while (runstate_ == RSRUN) {
    cc = rdtsc();
    if (cc > ccend)
      break;
    dt = cc -lcc;
    tsum += dt;
    if (likely(samples_++ > 0)) {
      if (unlikely (min_ > dt)) {
    min_ = dt;
    minidx_ = samples_ - 1;
      } else if (unlikely(max_ < dt)) {
    max_ = dt;
    maxidx_ = samples_ - 1;
      }
    }
    else {
      min_ = max_ = dt;
      minidx_ = maxidx_ = 0;
    }
    b = dt / conf.ccperbin_;
    if (unlikely(b > conf.bins_))
      b = conf.bins_ - 1;
    bins_[b].n_++;
    bins_[b].sum_ += dt;
    lcc = cc;
  }
  endcc_ = ccend = cc;
  runtimecc_ = endcc_ - startcc_;
  sched_param sp;
  memset(&sp, 0, sizeof(sp));
  if (conf.priority_ && sched_setscheduler(gettid(), SCHED_OTHER, &sp))
    perror("reset priority failed");

  if (id_) {
    __sync_sub_and_fetch(&active_, 1);
  } else {
    runstate_ = RSSTOP;
    while (active_ > 0)
      __sync_synchronize();
    runstate_ = RSEXIT;
  }
  avg_ = samples_ ? (tsum / samples_) : 0;
  if (id_ == 0)
    runstate_ = RSEXIT;

  usleep((cpuid_+1)*3000);
  return ccend - ccstart;
}

void HiccupsInfo::print_us()
{
  fprintf(stdout,
      "thread#: %d core#: %d samples: %ld  avg: %.4f min: %.4f (@%ld) max: %.4f (@%ld) cycles: %lu start: %lu end: %lu\n",
      id_, conf.cpus_[id_], samples_, 1.0 * avg_ / syscpus.ccpermicro_,
      1.0 * min_ / syscpus.ccpermicro_, minidx_,
      1.0 * max_ / syscpus.ccpermicro_, maxidx_,
      runtimecc_, startcc_, endcc_);
  if (conf.verbose_) {
    uint64_t dtsum = 0;
    for (int ii = 0; ii < conf.bins_; ii++)
      dtsum += bins_[ii].sum_;
    uint64_t psamples = 0, pdtsum = 0;
    for (int ii = 0; ii < conf.bins_; ii++) {
      if (bins_[ii].n_ == 0)
    continue;
      psamples += bins_[ii].n_;
      pdtsum += bins_[ii].sum_;
      fprintf(stdout,
          "  [%06.2f-%06.2f): %-14ld %-14ld %11.5f%%(%9.5f)  %8.5f%%(%9.5f)\n",
          1.0 * ii * conf.ccperbin_ / syscpus.ccpermicro_,
          1.0 * (ii+1) * conf.ccperbin_ / syscpus.ccpermicro_,
          bins_[ii].n_, samples_ - psamples,
          samples_ ? (100.0*bins_[ii].n_/samples_) : 0.0,
          psamples ? (100.0*psamples/samples_) : 0.0,
          (dtsum != 0) ? (100.0*bins_[ii].sum_/dtsum) : 0.0,
          pdtsum ? (100.0*pdtsum/dtsum) : 0.0);
    }
    fprintf(stdout, "\n");
  }
}
