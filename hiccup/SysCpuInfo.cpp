#include "SysCpuInfo.h"

void SysCpuInfo::getisolcpu()
{
  FILE* fp = fopen("/proc/cmdline", "r");
  char line[1024];
  char *p, *pe;

  while (fgets(line, sizeof(line), fp) != NULL)
    if ((p = strstr(line, "isolcpus=")) != NULL) {
      p += strlen("isolcpus=");
      for (pe = p; isdigit(*pe) || *pe == ',' || *pe == '-'; pe++)
    ;
      *pe = '\0';
      setcpus(p);
      fclose(fp);
      return;
    }
}

long SysCpuInfo::getccpermicro()
{
  uint64_t ccstart0, ccstart1, ccend0, ccend1, us, cc;
  timeval  todstart, todend;

  ccstart0 = rdtsc();
  gettimeofday(&todstart, 0);
  ccstart1 = rdtsc();
  usleep(10000);      // sleep for 10 milli seconds
  ccend0 = rdtsc();
  gettimeofday(&todend, 0);
  ccend1 = rdtsc();

  us = ( todend.tv_sec - todstart.tv_sec ) * 1000000UL + todend.tv_usec - todstart.tv_usec;
  cc = (ccend1 + ccend0) / 2 - (ccstart1 + ccstart0) / 2;

  float r = (double)cc * 1000 / us;

  // printf("get cpu clock: cc: %lu  us: %lu\n  cc/mico: %.4f\n  cc/mili: %.4f\n", cc, us, r / 1000.0, r);
  ccpersec_   = r * 1000;
  ccpermilli_ = r;
  ccpermicro_ = r / 1000;
  return r / 1000;
}

void SysCpuInfo::init()
{
  int maxcpus = sysconf(_SC_NPROCESSORS_ONLN);
  long r0, r1, r2;

  maxcpus_ = maxcpus;
  r0 = getccpermicro();
  r1 = getccpermicro();
  r2 = getccpermicro();
  if (r0 != r1 || r1 != r2) {
    fprintf(stderr, "Detected non stable clock rates: %ld %ld %ld\n", r0, r1, r2);
    // exit(1);
  }
}
