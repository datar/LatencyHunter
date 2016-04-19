

struct SysCpuInfo {
  int     maxcpus_;
  uint    ccpermicro_;
  uint    ccpermilli_;
  ulong   ccpersec_;

  void  init();
  void  getisolcpu();
  long  getccpermicro();
};