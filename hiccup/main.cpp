// Compile:
//  g++ -pthread -O3 -W -Wall -o hiccups hiccups.cpp -lpthread

#include "headers.h"

#include "HiccupConfig.h"
#include "SysCpuInfo.h"
#include "HiccupsInfo.h"

#define MILLIS_IN_SEC (1000UL)
#define MICROS_IN_SEC (1000000UL)
#define NANOS_IN_SEC  (1000000000UL)

using namespace std;

#define rdtsc() ({ register uint32_t lo, hi;            \
      __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));    \
      (uint64_t)hi << 32 | lo; })

static inline pid_t gettid() {
  return syscall(__NR_gettid); 
}

static void stackreserve(uint32_t sz = 64*1024);
static void runoncpu(int cn);

static inline void* memzalloc(unsigned long sz) { void* p = malloc(sz); memset(p, 0, sz); return p; }
static inline void  memfree(void* p) { free(p); }

#define likely(X)    __builtin_expect((X),1)
#define unlikely(X)  __builtin_expect((X),0)

// Hiccups parameters and data structures

const int MaxCpuCores = 256;        // Maximum cores the app supports, if more exist, change the  masks handling

const int BlockSize = (2*1024*1024);
#define HPGSZ(SZ) (((SZ)/BlockSize+1)*BlockSize)


static SysCpuInfo syscpus;

struct HiccupsInfo;

struct ThrdStart {
  HiccupsInfo*  hudata_;
  pthread_t thread_desc_;
  int           thrdid_;
  uint64_t      rv_;
};



static HiccupConfig conf;



volatile int HiccupsInfo::active_ __attribute__((aligned(0x40)));
volatile int HiccupsInfo::runstate_ __attribute__((aligned(0x40)));
HiccupsInfo* HiccupsInfo::hudatap_[MaxCpuCores];

static void setcpus(const char* cpuspec) {
  uint64_t mask = 0;
  char sep[64];
  int so = 0, x1 = 0, x2 = 0, c, lc = -1;

  while (sscanf(cpuspec + so, "%d%n%[ \t,\n]%n", &c, &x1, sep, &x2) >= 1) {
    if (lc >= 0 && c < 0) {
      c = -c;
      for (int ii = lc; ii < c; ii++)
    mask |= (1UL << ii);
    }
    if (c< 0 || c >= syscpus.maxcpus_) {
      fprintf(stderr, "cpu id out of range %d\n", c);
      exit(1);
    }
    mask |= (1UL << c);
    so += x2 ? x2 : x1;
    lc = c;
    x1 = x2 = 0;
  }
  for (int ii = 0; ii < 64; ii++)
    if (mask & (1UL << ii))
      conf.cpus_[conf.maxcpus_++] = ii;
  fprintf(stdout, "Running on %d cpu%s:\n", conf.maxcpus_, (conf.maxcpus_>1)?"s":"");
  for (int ii = 0; ii < conf.maxcpus_; ii++)
    fprintf(stdout, " %d", conf.cpus_[ii]);
  fprintf(stdout, "\n");
}


static const char* usagestr =
  "usage: hiccups [-v] [-b nano] [-c cpus-list] [-t seconds] [-n nice|-r RR|-f FF]\n"
  "nano - nano seconds per histogram bin\n"
  "cpulist - on which cpu cores to run\n"
  "seconds - how long to rung the test\n"
  "nice RR FF - priority and scheduling policy\n";

int main(int argc, char** argv)
{
  syscpus.init();
  
  if (mlockall(MCL_CURRENT | MCL_FUTURE))
    fprintf(stderr, "failed to lock all memory (ignored)\n");
  
  int oc;
  while ((oc = getopt(argc, argv, "hb:vr:t:p:f:c:n:")) != -1) {
    switch (oc) {
    case 'v':
      conf.verbose_++;
      break;
    case 'b': // nano seconds per bin. (10ns - 5000ns).
      conf.verbose_++;
      conf.resolution_ = atoi(optarg);
      break;
    case 't':
      conf.runtime_ = atoi(optarg);
      break;
    case 'n':
      conf.policy_ = SCHED_OTHER;
      conf.priority_ = atoi(optarg);
      break;
    case 'r':
      conf.policy_ = SCHED_RR;
      conf.priority_ = atoi(optarg);
      break;
    case 'f':
      conf.policy_ = SCHED_FIFO;
      conf.priority_ = atoi(optarg);
      break;
    case 'c':
      setcpus(optarg);
      break;
    case 'h':
    default:
      fprintf(stderr, "%s\n", usagestr);
      exit(0);
    }
  }

  conf.init();
  if (conf.maxcpus_ < 1) {
    fprintf(stderr, "could not find isolcpus=.. and no cpus '-c' defined on command line\n%s\n", usagestr);
    exit(1);
  }
  runoncpu(conf.cpus_[0]);
  fprintf(stdout, "main thread on cpu core#: %d\n", conf.cpus_[0]);
  conf.print();
  ThrdStart ta[MaxCpuCores];
  memset(&ta, 0, sizeof (ta));
  for (int ii = 1; ii < conf.maxcpus_; ii++) {
    ta[ii].thrdid_ = ii;
    pthread_create(&ta[ii].thread_desc_, 0, HiccupsInfo::runthread, &ta[ii]);
  }
  HiccupsInfo::runthread(&ta[0]);
  for (int ii = 0; ii < conf.maxcpus_; ii++) {
    ta[ii].hudata_->print_us();
  }
}

static void runoncpu(int cpu)
{
  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);

  if (sched_setaffinity(gettid(), sizeof(cpu_set_t), &cpuset) != 0) {
    fprintf(stderr, "failed to set cpu affinity: %d, errno: %d '%s'\n",
        cpu, errno, strerror(errno));
    exit(1);
  }
}

static void stackreserve(const uint32_t sz)
{
  volatile char dummy[sz];

  memset((void*)dummy, 0, sz);
}

