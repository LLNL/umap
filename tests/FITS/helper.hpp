#ifndef UMAP_HELPER_HPP
#define UMAP_HELPER_HPP

#include <cmath>
#include <omp.h>
#include <iostream>
#include "fitsio.h"
#include "umaptest.h"

struct patch {
  uint64_t sx, sy, ex, ey;  // boundries for each image
};

struct helper_funs {
  umt_optstruct_t options;
  int bitpix;
  int naxis;
  long naxes[2];
  LONGLONG headstart;
  LONGLONG datastart;
  LONGLONG dataend;

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

  int get_fits_image_info(const std::string& filename)
  {
    fitsfile* fptr = NULL;
    int status = 0;

    if ( fits_open_file(&fptr, filename.c_str(), READONLY, &status) ) {
      fits_report_error(stderr, status);
      return -1;
    }

    if ( fits_get_hduaddrll(fptr, &headstart, &datastart, &dataend, &status) ) {
      fits_report_error(stderr, status);
      return -1;
    }

    if ( fits_get_img_type(fptr, &bitpix, &status) ) {
      fits_report_error(stderr, status);
      return -1;
    }

    if ( fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status) ) {
      fits_report_error(stderr, status);
      return -1;
    }

    if ( fits_close_file(fptr, &status) ) {
      fits_report_error(stderr, status);
      return -1;
    }
    return 0;
  }
};

#endif
