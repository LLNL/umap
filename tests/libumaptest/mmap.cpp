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
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "umap.h"
#include "umaptest.h"

using namespace std;

int umt_openandmap(const umt_optstruct_t* testops, uint64_t numbytes, void** region)
{
  int fd;
  int open_options = O_RDWR;

  if (testops->iodirect) 
    open_options |= O_DIRECT;

  if ( !testops->noinit )
    open_options |= O_CREAT;

#ifdef O_LARGEFILE
    open_options |= O_LARGEFILE;
#endif

  fd = open(testops->fn, open_options, S_IRUSR|S_IWUSR);
  if(fd == -1) {
    perror("open");
    exit(-1);
  }

  if (testops->noinit) {
    // If we are not initializing file, make sure that it is big enough
    struct stat sbuf;

    if (fstat(fd, &sbuf) == -1) {
      perror("fstat");
      exit(-1);
    }

    if ((uint64_t)sbuf.st_size < numbytes) {
      cerr << testops->fn 
        << " file is not large enough.  "  << sbuf.st_size 
        << " < size requested " << numbytes << endl;
      exit(-1);
    }
  }

  try {
      int x;
    if((x = posix_fallocate(fd,0, numbytes) != 0)) {
      perror("??posix_fallocate");

      cerr << "posix_fallocate(" << fd << ", 0, " << numbytes << ") returned " << x << endl;
      exit(-1);
    }
  } catch(const std::exception& e) {
      cerr << "posix_fallocate: " << e.what() << endl;
      exit(-1);
  } catch(...) {
      cerr << "posix_fallocate failed to instantiate _umap object\n";
      exit(-1);
  }

  int prot = PROT_READ|PROT_WRITE;

  if ( testops->usemmap ) {
    int flags = MAP_SHARED;

    *region = mmap(NULL, numbytes, prot, flags, fd, 0);
    if (*region == MAP_FAILED) {
      perror("mmap");
      exit(-1);
    }
  }
  else {
    int flags = UMAP_PRIVATE;

    *region = umap(NULL, numbytes, prot, flags, fd, 0);
    if (*region == UMAP_FAILED) {
      perror("umap");
      exit(-1);
    }
  }

  return fd;
}

void umt_closeandunmap(const umt_optstruct_t* testops, uint64_t numbytes, void* region, int fd)
{
  if ( testops->usemmap ) {
    if (munmap(region, numbytes) < 0) {
      perror("munmap");
      exit(-1);
    }
  }
  else {
    if (uunmap(region, numbytes) < 0) {
      perror("uunmap");
      exit(-1);
    }
  }

  close(fd);
}

//-------support fits files ----------------
void* umt_openandmap_fits(const umt_optstruct_t* testops, uint64_t numbytes, void** region,off_t offset,off_t data_size)
{
  char* filename;
  char num[5];
  int open_options = O_RDWR;

  umap_backing_file *fits_files;
  if (testops->iodirect) 
    open_options |= O_DIRECT;

  if ( !testops->noinit )
    open_options |= O_CREAT;

#ifdef O_LARGEFILE
    open_options |= O_LARGEFILE;
#endif

  if (testops->fnum==-1)
  {
      perror("number of files not in input");
      exit(-1);
  }
  fits_files=(umap_backing_file *)calloc(testops->fnum,sizeof(umap_backing_file));
  filename=(char *)std::malloc(sizeof(char)*100);

  for (int i=0;i<testops->fnum;i++)
  {
        strcpy(filename,testops->fn);
	sprintf(num,"%d",i+1);
	strcat(filename,num);
	strcat(filename,".fits");

	fits_files[i].fd = open(filename, open_options, S_IRUSR|S_IWUSR);
	//printf("processing %s, %d\n",filename,fdlist[i]);

	if(fits_files[i].fd == -1) 
	{
	    perror("open");
	    exit(-1);
	}
	fits_files[i].data_size=data_size;
	fits_files[i].data_offset=offset;
  }

  if (testops->noinit) {
    // If we are not initializing file, make sure that it is big enough
    struct stat sbuf;
    uint64_t totalsize=0;
    for (int i=0;i<testops->fnum;i++){
	if (fstat(fits_files[i].fd, &sbuf) == -1){
        perror("fstat");
        exit(-1);
      }
      //printf("size:%d\n",sbuf.st_size);
      totalsize+=(uint64_t)sbuf.st_size;
    }

    if (totalsize < numbytes) {
       cerr << testops->fn 
        << " file is not large enough.  "  << sbuf.st_size 
        << " < size requested " << numbytes << endl;
      exit(-1);
    }
  }

    int prot = PROT_READ|PROT_WRITE;
    int flags = UMAP_PRIVATE;

    *region = umap_mf(NULL, numbytes, prot, flags, testops->fnum, fits_files);
    if (*region == UMAP_FAILED) {
      perror("umap");
      exit(-1);
    }

    return (void *)fits_files;
}

void umt_closeandunmap_fits(const umt_optstruct_t* testops, uint64_t numbytes, void* region,void* p)
{
  if ( testops->usemmap ) {
    if (munmap(region, numbytes) < 0) {
      perror("munmap");
      exit(-1);
    }
  }
  else {
    if (uunmap(region, numbytes) < 0) {
      perror("uunmap");
      exit(-1);
    }
  }
  umap_backing_file *fits_files=(umap_backing_file *)p;
  for (int i=0;i<testops->fnum;i++)
  close(fits_files[i].fd);
  free(fits_files);
}

//-------support fits file (private)------------
void* umt_openandmap_fits2(const umt_optstruct_t* testops, uint64_t numbytes, void** region,off_t offset,off_t data_size)
{
  umap_backing_file* fits_files;
  char* filename;
  char num[5];
  int open_options = O_RDWR;

  if (testops->iodirect) 
    open_options |= O_DIRECT;

  if ( !testops->noinit )
    open_options |= O_CREAT;

#ifdef O_LARGEFILE
    open_options |= O_LARGEFILE;
#endif

  if (testops->fnum==-1)
  {
      perror("number of files not in input");
      exit(-1);
  }
  fits_files=(umap_backing_file *)calloc(testops->fnum,sizeof(umap_backing_file));
  filename=(char *)std::malloc(sizeof(char)*100);

  for (int i=0;i<testops->fnum;i++)
  {
        strcpy(filename,testops->fn);
	sprintf(num,"%d",i+1);
	strcat(filename,num);
	strcat(filename,".fits");

	fits_files[i].fd = open(filename, open_options, S_IRUSR|S_IWUSR);
	//printf("processing %s, %d\n",filename,fdlist[i]);

	if(fits_files[i].fd == -1) 
	{
	    perror("open");
	    exit(-1);
	}
  }

  if (testops->noinit) {
    // If we are not initializing file, make sure that it is big enough
    struct stat sbuf;
    for (int i=0;i<testops->fnum;i++){
      if (fstat(fits_files[i].fd, &sbuf) == -1){
        perror("fstat");
        exit(-1);
      }
      //printf("size:%d\n",sbuf.st_size);
      if ((uint64_t)sbuf.st_size < numbytes) {
        cerr << testops->fn 
        << " file is not large enough.  "  << sbuf.st_size 
        << " < size requested " << numbytes << endl;
        exit(-1);
      }
    }
  }

    int prot = PROT_READ|PROT_WRITE;
    int flags = UMAP_PRIVATE;

    for (int i=0;i<testops->fnum;i++)
    {
      region[i] = umap(NULL, numbytes, prot, flags, fits_files[i].fd, 0);
      if (region[i] == UMAP_FAILED) 
      {
	perror("umap");
	exit(-1);
      }
    }

    return (void *)fits_files;
}

void umt_closeandunmap_fits2(const umt_optstruct_t* testops, uint64_t numbytes, void** region,void* p)
{
  if ( testops->usemmap ) {
    if (munmap(region, numbytes) < 0) {
      perror("munmap");
      exit(-1);
    }
  }
  else {
    for (int i=0;i<testops->fnum;i++)
    if (uunmap(region[i], numbytes) < 0) {
      perror("uunmap");
      exit(-1);
    }
  }
  umap_backing_file *fits_files=(umap_backing_file *)p;
  for (int i=0;i<testops->fnum;i++)
  close(fits_files[i].fd);
  free(fits_files);
}
