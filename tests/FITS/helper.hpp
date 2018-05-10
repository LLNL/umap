#ifndef UMAP_HELPER_HPP
#define UMAP_HELPER_HPP

#include <cmath>
#include <iostream>
#include "omp.h"
#include "fitsio.h"
#include "testoptions.h"

struct patch {
  uint64_t sx, sy, ex, ey;  // boundries for each image
};

struct helper_funs {
  umt_optstruct_t options;

  inline double gets(void) { return omp_get_wtime(); }
  inline bool fequal(double a, double b) { return ( fabs(a-b) < (1e-6) ) ? 1 : 0; }

  inline void swapbyte(float *a, float *b)
  {
    char *a1=(char *)a;
    char *b1=(char *)b;
    b1[0] = a1[3];
    b1[3] = a1[0];
    b1[1] = a1[2];
    b1[2] = a1[1];
  }

  void displaycube(double *cube, struct patch *list, int n)
  {
    uint64_t lx = list[0].ex;
    for ( int k = 1; k <= n; k++ ) {
      for ( unsigned int i = list[k].sy; i < list[k].ey; i++ ) {// bounding box
        for (unsigned int j=list[k].sx; j<list[k].ex; j++) {
          std::cout << cube[i*lx+j] << std::endl;
        }
      }
    }
  }
};

#endif
