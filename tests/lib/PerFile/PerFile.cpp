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
#include <unordered_map>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include "umap.h"
#include "testoptions.h"
#include "PerFile.h"

using namespace std;

typedef struct umt_map_handle {
  uint64_t range_size;
  string filename;
  int fd;
} umt_map_handle;

static unordered_map<void*, umt_map_handle*> mappings;

static ssize_t pstore_read(void* region, void* buf, size_t nbytes, off_t region_offset)
{
  ssize_t rval;
  auto it = mappings.find(region);
  assert( it != mappings.end() );
  umt_map_handle* handle = it->second;

  if ( ( rval = pread(handle->fd, buf, nbytes, region_offset) ) == -1) {
    perror("ERROR: pread failed");
    exit(1);
  }

  return rval;
}

static ssize_t pstore_write(void* region, void* buf, size_t nbytes, off_t region_offset)
{
  ssize_t rval;
  auto it = mappings.find(region);

  if ( it == mappings.end() ) {
    cerr << __FUNCTION__ << "(region=" << region << ", buf=" << buf << ", nbytes=" << nbytes << ", offset=" << region_offset << ")\n";
    assert( "Unable to find region in map" && it != mappings.end() );
  }
  umt_map_handle* handle = it->second;

  if ( ( rval = pwrite(handle->fd, buf, nbytes, region_offset) ) == -1) {
    perror("ERROR: pwrite failed");
    assert(0);
  }
  return rval;
}

void* PerFile_openandmap(const umt_optstruct_t* testops, uint64_t numbytes)
{
  void* region = NULL;
  umt_map_handle* handle = new umt_map_handle;
  int open_options = O_RDWR | O_LARGEFILE;
  string filename(testops->filename);

  if ( testops->iodirect ) 
    open_options |= O_DIRECT;

  if ( !testops->noinit )
    open_options |= O_CREAT;

  handle->range_size = numbytes;
  handle->filename = filename;
  if ( ( handle->fd = open(filename.c_str(), open_options, S_IRUSR | S_IWUSR) ) == -1 ) {
    string estr = "Failed to open/create " + handle->filename + ": ";
    perror(estr.c_str());
    return NULL;
  }

  if ( open_options & O_CREAT ) { // If we are initializing, attempt to pre-allocate disk space for the file.
    try {
      int x;
      if ( ( x = posix_fallocate(handle->fd, 0, handle->range_size) != 0 ) ) {
        ostringstream ss;
        ss << "Failed to pre-allocate " << handle->range_size << " bytes in " << handle->filename << ": ";
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
  if (fstat(handle->fd, &sbuf) == -1) {
    string estr = "Failed to get status (fstat) for " + filename + ": ";
    perror(estr.c_str());
    return NULL;
  }

  if ( (off_t)sbuf.st_size != (handle->range_size) ) {
    cerr << filename << " size " << sbuf.st_size << " does not match specified data size of " << (handle->range_size) << endl;
    return NULL;
  }

  const int prot = PROT_READ|PROT_WRITE;

  if ( testops->usemmap ) {
    region = mmap(NULL, handle->range_size, prot, MAP_PRIVATE | MAP_NORESERVE, handle->fd, 0);
    if (region == MAP_FAILED) {
      ostringstream ss;
      ss << "mmap of " << handle->range_size << " bytes failed for " << handle->filename << ": ";
      perror(ss.str().c_str());
      return NULL;
    }
  }
  else {
    int flags = UMAP_PRIVATE;

    region = umap(NULL, handle->range_size, prot, flags, pstore_read, pstore_write);
    if ( region == UMAP_FAILED ) {
        ostringstream ss;
        ss << "umap_mf of " << handle->range_size << " bytes failed for " << handle->filename << ": ";
        perror(ss.str().c_str());
        return NULL;
    }
  }

  mappings[region] = handle;
  return region;
}

void PerFile_closeandunmap(const umt_optstruct_t* testops, uint64_t numbytes, void* region)
{
  auto it = mappings.find(region);
  assert( "Unable to find region mapping" && it != mappings.end() );
  umt_map_handle* handle = it->second;

  if ( testops->usemmap ) {
    if ( munmap(region, handle->range_size) < 0 ) {
      ostringstream ss;
      ss << "munmap of " << numbytes << " bytes failed for " << handle->filename << "on region " << region << ": ";
      perror(ss.str().c_str());
      exit(-1);
    }
  }
  else {
    if (uunmap(region, numbytes) < 0) {
      ostringstream ss;
      ss << "uunmap of " << numbytes << " bytes failed for " << handle->filename << "on region " << region << ": ";
      perror(ss.str().c_str());
      exit(-1);
    }
  }

  close(handle->fd);

  mappings.erase(region);   // Don't remove region mapping until after uumap has flushed data
  delete handle;
}
