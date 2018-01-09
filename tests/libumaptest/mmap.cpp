/*
This file is part of UMAP.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/LLNL/umap/blob/master/COPYRIGHT
This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free
Software Foundation) version 2.1 dated February 1999.  This program is
distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the terms and conditions of the GNU Lesser General Public License
for more details.  You should have received a copy of the GNU Lesser General
Public License along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include "umap.h"
#include "umaptest.h"

using namespace std;

typedef struct umt_map_handle {
  uint64_t total_range_size;
  vector<string> files;
  vector<umap_backing_file> mf_files;
} umt_map_handle;

void* umt_openandmap(const umt_optstruct_t* testops, uint64_t numbytes, void** region)
{
  return umt_openandmap_mf(testops, numbytes, region, (off_t)0, numbytes);
}

void umt_closeandunmap(const umt_optstruct_t* testops, uint64_t numbytes, void* region, void* p)
{
  umt_closeandunmap_mf(testops, numbytes, region, p);
}

//
// Usage:
//   If testops->num_files is 1, the testops->filname is assumed to point
//   to the full name of the file.
//
//   Otherwise, if testops->num_files > 1, the testops->filename is
//   assumed to be a prefix and the first file is assumed to contain
//   the FITS header.  The test programs use the header located in the
//   first file to determine the data size.  The implementation ASSUMES
//   that all of the data files are the same size and that the size
//   is page-aligned.
//
//   For 3 FITS files with "foo" prefix, the implementation will look for
//   the following files:
//
//   foo1.fits - 1st file containing BOTH FITS Header and Data
//   foo1.data - 1st file containing only data (use exdata_fits to generate)
//   foo2.data - 2nd file containing only data
//   foo3.data - 3rd file containing only data
//
void* umt_openandmap_mf(const umt_optstruct_t* testops, uint64_t numbytes, void** region, off_t offset, off_t data_size)
{
  int open_options = O_RDWR | O_LARGEFILE;  // TODO: Handle READONLY case someday
  umt_map_handle* handle = new umt_map_handle;

  offset = 0;     // Hack for now until we determine how to distinguish files without headers.

  if ( testops->iodirect ) 
    open_options |= O_DIRECT;

  if ( !testops->noinit )
    open_options |= O_CREAT;

  handle->total_range_size = 0;

  for ( int i = 0; i < testops->num_files; ++i ) {
    string filename;
    umap_backing_file bfile;

    {
      ostringstream ss;
      if (testops->num_files > 1) // Treat file name as a basename
        ss << testops->filename << (i+1) << ".data";
      else
        ss << testops->filename;
      filename = ss.str();
    }

    if ( ( bfile.fd = open(filename.c_str(), open_options, S_IRUSR | S_IWUSR) ) == -1 ) {
      string estr = "Failed to open/create " + filename + ": ";
      perror(estr.c_str());
      return NULL;
    }

    if ( ! testops->noinit ) { // If we are initializing, attempt to pre-allocate disk space for the file.
      try {
        int x;
        if ( ( x = posix_fallocate(bfile.fd, 0, data_size) != 0 ) ) {
          ostringstream ss;
          ss << "Failed to pre-allocate " << data_size << " bytes in " << filename << ": ";
          perror(ss.str().c_str());
          return NULL;
        }
      } catch(const std::exception& e) {
        cerr << "posix_fallocate: " << e.what() << endl;
        return NULL;
      } catch(...) {
        cerr << "posix_fallocate failed to instantiate _umap object\n";
        return NULL;
      }
    }

    struct stat sbuf;
    if (fstat(bfile.fd, &sbuf) == -1) {
      string estr = "Failed to get status (fstat) for " + filename + ": ";
      perror(estr.c_str());
      return NULL;
    }

    if ( (off_t)sbuf.st_size != (data_size+offset) ) {
      cerr << filename << " size " << sbuf.st_size << " does not match specified data size of " << (data_size+offset) << endl;
      return NULL;
    }

    handle->total_range_size += (uint64_t)data_size;
    bfile.data_size = data_size;
    bfile.data_offset = offset;
    handle->mf_files.push_back(bfile);
    handle->files.push_back(filename);
  }

  const int prot = PROT_READ|PROT_WRITE;

  if ( testops->usemmap ) {
    void* next_mmap = mmap(NULL, handle->total_range_size, prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (next_mmap == MAP_FAILED) {
      ostringstream ss;
      ss << "reservation (mmap) of " << handle->total_range_size << " bytes failed: ";
      perror(ss.str().c_str());
      return NULL;
    }

    *region = next_mmap;

    if ( munmap(next_mmap, handle->total_range_size) < 0 ) {
      ostringstream ss;
      ss << "reservation (mumap) of " << handle->total_range_size << " from " << next_mmap << " failed: ";
      perror(ss.str().c_str());
      return NULL;
    }

    //cout << "Starting contiguous mappings at: " << next_mmap << endl;

    for ( int i = 0; i < testops->num_files; ++i ) {
      void* mmap_region;
      mmap_region = mmap(next_mmap, handle->mf_files[i].data_size, prot, MAP_PRIVATE | MAP_FIXED | MAP_NORESERVE, handle->mf_files[i].fd, offset);
      if (mmap_region == MAP_FAILED) {
        ostringstream ss;
        ss << "mmap of " << handle->mf_files[i].data_size << " bytes failed for " << handle->files[i] << ": ";
        perror(ss.str().c_str());
        return NULL;
      }
      //cout << handle->files[i] << "\t" << next_mmap << "\t" << mmap_region << endl;

      assert(mmap_region == next_mmap);
      next_mmap = static_cast<char*>(mmap_region) + handle->mf_files[i].data_size;
    }
  }
  else {
    int flags = UMAP_PRIVATE;

    *region = umap_mf(NULL, handle->total_range_size, prot, flags, testops->num_files, &handle->mf_files[0]);
    if ( *region == UMAP_FAILED ) {
        ostringstream ss;
        ss << "umap_mf of " << handle->total_range_size << " bytes failed for " << handle->files[0] << ": ";
        perror(ss.str().c_str());
        return NULL;
    }
    //cout << handle->files[0] << "\t" << handle->total_range_size << " bytes allocated at " << *region << endl;
  }

  //umt_closeandunmap_mf(testops, handle->total_range_size, *region, handle);

  //exit(0);
  return (void *)handle;
}

void umt_closeandunmap_mf(const umt_optstruct_t* testops, uint64_t numbytes, void* region, void* p)
{
  umt_map_handle* handle = static_cast<umt_map_handle*>(p);

  if ( testops->usemmap ) {
    int cnt = 0;
    for ( auto i : handle->mf_files ) {
      //cout << "munmap(region=" << region << ", size=" << i.data_size << ") for file " << handle->files[ cnt ] << endl;
      if ( munmap(region, i.data_size) < 0 ) {
        ostringstream ss;
        ss << "munmap of " << i.data_size << " bytes failed for " << handle->files[0] << "on region " << region << ": ";
        perror(ss.str().c_str());
        exit(-1);
      }
      cnt++;
      region = static_cast<char*>(region) + i.data_size;
    }
  }
  else {
    //cout << "uunmap(region=" << region << ", size=" << numbytes << ") for file " << handle->files[0] << endl;
    if (uunmap(region, numbytes) < 0) {
      ostringstream ss;
      ss << "munmap of " << numbytes << " bytes failed for " << handle->files[0] << "on region " << region << ": ";
      perror(ss.str().c_str());
      exit(-1);
    }
  }

  for ( auto i : handle->mf_files )
    close(i.fd);

  delete handle;
}
