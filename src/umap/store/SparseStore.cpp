//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <atomic>
#include <string.h>

#include <umap/umap.h>
#include <umap/store/SparseStore.h>

namespace Umap {
    SparseStore::SparseStore(size_t _rsize_, size_t _aligned_size_, std::string _root_path_, size_t _file_Size_)
      : rsize{_rsize_}, aligned_size{_aligned_size_}, root_path{_root_path_}, file_size{_file_Size_}{
      // Round file size to be multiple of page size
      file_size = aligned_size*ceil(file_size*1.0/aligned_size);
      num_files = (uint64_t) ceil( rsize*1.0 / file_size );
      numreads = numwrites = 0;
      file_descriptors = new file_descriptor[num_files];
      for (int i = 0 ; i < num_files ; i++){
        file_descriptors[i].id = -1;
      }
      DIR *directory;
      struct dirent *ent;
      std::string metadata_file_path = root_path + "/_metadata";
      if ((directory = opendir(root_path.c_str())) != NULL){
        directory_creation_status = 0;
        // Get stored file size
        std::ifstream metadata(metadata_file_path.c_str());
        if (!metadata.is_open()){
          std::cout << "ERROR: Failed to open metadata file" << std::endl;
        }
        else{
          metadata >> file_size;
        }
      }
      else{
        directory_creation_status = mkdir(root_path.c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        std::ofstream metadata(metadata_file_path.c_str());
        if (!metadata.is_open()){
          std::cout << "ERROR: Failed to open metadata file" << std::endl;
        }
        else{
          metadata << file_size;
        }
      }
      if (directory_creation_status != 0){
        std::cout << "ERROR: Failed to create directory" << std::endl;
      }
    }

    SparseStore::~SparseStore(){
      std::cout << "SparseStore Total Reads: " << numreads << std::endl;
      std::cout << "SparseStore Total Writes: " << numwrites << std::endl;
      for (int i = 0 ; i < num_files ; i++){
        if (file_descriptors[i].id != -1){
          if (close(file_descriptors[i].id) != 0){
            perror("Failed to close file");
          }
        }
      }
      delete [] file_descriptors;
    }

    ssize_t SparseStore::read_from_store(char* buf, size_t nb, off_t off) {
      ssize_t read = 0;
      off_t file_offset;
      int fd = get_fd(off, file_offset); 
      read = pread(fd,buf,nb,file_offset);
      if(read == -1){
        std::cerr << "pread(fd=" << fd << ", buff=" << (void*)buf <<  ", nb=" << nb << ", off=" << off << ") Failed - " << strerror(errno) << std::endl;
      }
      numreads++;
      return read;
    }

    ssize_t SparseStore::write_to_store(char* buf, size_t nb, off_t off) {
      ssize_t written = 0;
      off_t file_offset;
      int fd = get_fd(off, file_offset);
      written = pwrite(fd,buf,nb,file_offset);
      if(written == -1){
        std::cerr << "pwrite(fd=" << fd << ", buff=" << (void*)buf <<  ", nb=" << nb << ", off=" << off << ") Failed - " << strerror(errno) << std::endl;
      }
      numwrites++;
      return written;
    }

    int SparseStore::get_directory_creation_status(){
      return directory_creation_status;
    }

    int SparseStore::get_fd(off_t offset, off_t &file_offset){
      int fd_index = offset / file_size;
      file_offset = offset % file_size; 
      std::string filename = root_path + "/" + std::to_string(fd_index);
      if ( file_descriptors[fd_index].id == -1 ){
            creation_mutex.lock(); // Grab mutex (only in case of creating new file, rather than serializing a larger protion of the code)
            if (file_descriptors[fd_index].id == -1){ // Recheck the value to make sure that another thread did not already create the file
              int fd;
              if ((fd = open(filename.c_str(), O_CREAT | O_RDWR | O_LARGEFILE | O_DIRECT, S_IRUSR | S_IWUSR)) != -1){
                int fallocate_status;
                if( (fallocate_status = posix_fallocate(fd,0,file_size) ) == 0){
                  file_descriptors[fd_index].id = fd;
                }
                else{
                  std::cout << "ERROR: fallocate() failed for file with id: " << fd_index << std::endl;
                  perror("");
                }
              }
              else{
                std::cout << "ERROR: Failed to open file with id: " << fd_index << std::endl;
                perror("");
              }
            }
            creation_mutex.unlock(); // Release mutex
      }
      return file_descriptors[fd_index].id;
    }
}
