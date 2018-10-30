/* This file is part of UMAP.
 *
 * It is a simple example showing how an application may map to a a file,
 * Initialize the file with data, sort the data, then verify that sort worked
 * correctly.
 *
 * For copyright information see the COPYRIGHT file in the top level directory,
 * or at https://github.com/LLNL/umap/blob/master/COPYRIGHT. This program is
 * free software; you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License (as published by the Free Software
 * Foundation) version 2.1 dated February 1999.  This program is distributed in
 * the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the terms and conditions of the GNU Lesser General Public License for
 * more details.  You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <iostream>
#include <parallel/algorithm>
#include <fcntl.h>
#include <omp.h>
#include "umap.h"

using namespace std;

int main(int argc, char **argv)
{
  const char* filename = "/tmp/testfile";
  uint64_t totalbytes = 1000*umap_cfg_get_pagesize();
  uint64_t arraysize = totalbytes / sizeof(uint64_t);

  unlink(filename);   // Remove the file if it exists

  int fd = open(filename,
                O_RDWR | O_LARGEFILE | O_DIRECT | O_CREAT,
                S_IRUSR | S_IWUSR);
  if ( fd == -1 ) {
    cerr << "Failed to create " << filename << endl;
    return -1;
  }

  void* base_addr = umap( NULL,
                          totalbytes,
                          PROT_READ|PROT_WRITE,
                          UMAP_PRIVATE,
                          fd,
                          0);
  if ( base_addr == UMAP_FAILED ) {
    cerr << "umap failed\n";
    return -1;
  }

  cout << "Initializing Array\n";
  uint64_t *arr = (uint64_t *) base_addr;

#pragma omp parallel for
  for(uint64_t i=0; i < arraysize; ++i)
    arr[i] = (uint64_t) (arraysize - i);

  cout << "Sorting Data\n";
  __gnu_parallel::sort( arr,
                        &arr[arraysize],
                        std::less<uint64_t>(),
                        __gnu_parallel::quicksort_tag());

  cout << "Validating Data\n";
#pragma omp parallel for
  for(uint64_t i = 0; i < arraysize; ++i) {
    if (arr[i] != (i+1))
      cerr << "Data miscompare\n";
  }

  if (uunmap(base_addr, totalbytes) < 0) {
    cerr << "uunamp failed\n";
    return -1;
  }
  return 0;
}
