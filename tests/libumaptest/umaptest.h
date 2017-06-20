#ifndef _UMAPTEST_H
#define _UMAPTEST_H
#include <cstdint>

typedef struct {
  int initonly;
  int noinit;
  int iodirect;
  int usemmap;

  uint64_t numpages;
  uint64_t numthreads;
  uint64_t bufsize;
  char const* fn;
} umt_optstruct_t;

extern "C" {
  void umt_getoptions(umt_optstruct_t&, int, char *argv[]);
  void umt_openandmap(const umt_optstruct_t&, uint64_t, int&, void*&);
}
#endif // _UMAPTEST_H
