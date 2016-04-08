#ifndef _HICCUPSINFO_H_
#define _HICCUPSINFO_H_


struct HiccupsInfo  // Hiccup Detection Thread
{
  enum { RSERROR, RSWAIT, RSRUN, RSSTOP, RSEXIT };
  uint64_t run();
  void print_us();

  static void* runthread(void* vp);

  uint64_t min_, minidx_,
    max_, maxidx_,
    avg_, samples_,
    runtimecc_, startcc_, endcc_, runtimeus_;
  int id_, cpuid_; // id - thread ID, cpuid_ - CPU core ID
  struct HUbin {
    uint64_t n_, sum_;
  };
  HUbin* bins_; // Hiccup bins, allocated, two
  
  static HiccupsInfo* hudatap_[];
  static volatile int tststate_;
  static int numcores_;
  static volatile int active_;
  static volatile int runstate_;
};

#endif