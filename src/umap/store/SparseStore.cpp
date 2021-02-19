//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2021 Lawrence Livermore National Security, LLC and other
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
#include <umap/util/Macros.hpp>

namespace Umap {

    // Create mode
    SparseStore::SparseStore(size_t _rsize_, size_t _aligned_size_, std::string _root_path_, size_t _file_Size_)
      : rsize{_rsize_}, aligned_size{_aligned_size_}, root_path{_root_path_}, file_size{_file_Size_}{
      
      // Round file size to be multiple of page size
      file_size = aligned_size*ceil( file_size*1.0/aligned_size );
      num_files = (uint64_t) ceil( rsize*1.0 / file_size );
      numreads = numwrites = 0;
      read_only = false;
      file_descriptors = new file_descriptor[num_files];
      for (int i = 0 ; i < num_files ; i++){
        file_descriptors[i].id = -1;
      }
      DIR *directory;
      struct dirent *ent;
      std::string metadata_file_path = root_path + "/_metadata";
      if ((directory = opendir(root_path.c_str())) != NULL){
        UMAP_ERROR("Directory already exist. Needs to be opened in open mode: store = new SparseStore(root_path,is_read_only); ");
      }
      else{
        int directory_creation_status = mkdir(root_path.c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (directory_creation_status != 0){
          UMAP_ERROR("ERROR: Failed to create directory" << " - " << strerror(errno));
        }
        std::ofstream metadata(metadata_file_path.c_str());
        if (!metadata.is_open()){
          UMAP_ERROR("Failed to open metadata file" << " - " << strerror(errno));
        }
        else{
          metadata << file_size << std::endl;
          // set current capacity to be the file granularity
          metadata << std::max(file_size,rsize);
        }
      }
    }

    // Open mode
    SparseStore::SparseStore(std::string _root_path_, bool _read_only_)
    : root_path{_root_path_}, read_only{_read_only_}{
      // Check if directory exists, if not, throw error
      DIR *directory;
      struct dirent *ent;
      if ((directory = opendir(root_path.c_str())) == NULL){
        UMAP_ERROR("Base directory does not exist" << " - " << strerror(errno));
      }
      else{
        // Get stored file size and current capacity
        std::string metadata_file_path = root_path + "/_metadata";
        std::ifstream metadata(metadata_file_path.c_str());
        if (!metadata.is_open()){
          UMAP_ERROR("Failed to open metadata file" << " - " << strerror(errno));
        }
        else{
          metadata >> file_size;
          metadata >> current_capacity;
          numreads = numwrites = 0;
          num_files = (uint64_t) ceil( current_capacity*1.0 / file_size );
          file_descriptors = new file_descriptor[num_files];
          for (int i = 0 ; i < num_files ; i++){
            file_descriptors[i].id = -1;
          }
        }
      }

      closedir(directory);

    }
    

    SparseStore::~SparseStore(){
      UMAP_LOG(Info,"SparseStore Total Reads: " << numreads);
      UMAP_LOG(Info,"SparseStore Total Writes: " << numwrites); 
      delete [] file_descriptors;
    }

    ssize_t SparseStore::read_from_store(char* buf, size_t nb, off_t off) {
      ssize_t read = 0;
      off_t file_offset;
      int fd = get_fd(off, file_offset); 
      read = pread(fd,buf,nb,file_offset);
      if(read == -1){
        UMAP_ERROR("pread(fd=" << fd << ", buff=" << (void*)buf <<  ", nb=" << nb << ", off=" << off << ") Failed - " << strerror(errno));
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
        UMAP_ERROR("pwrite(fd=" << fd << ", buff=" << (void*)buf <<  ", nb=" << nb << ", off=" << off << ") Failed - " << strerror(errno));
      }
      numwrites++;
      return written;
    }

    int SparseStore::close_files(){
      int return_status = 0;
      for (int i = 0 ; i < num_files ; i++){
        if (file_descriptors[i].id != -1){
          int close_status = close(file_descriptors[i].id);
          if (close_status != 0){
            UMAP_LOG(Warning,"SparseStore: Failed to close file with id: " << i << " - " << strerror(errno));
          }
          return_status = return_status | close_status;
        }  
      }
      return return_status;
    }

    size_t SparseStore::get_current_capacity(){
      return current_capacity;
    }
    
    /**
     * To get the size of any persistent region created using SparseStore without the need to instianiate an object
    **/ 
    size_t SparseStore::get_capacity(std::string base_path){
      size_t capacity = 0;
      std::string metadata_path = base_path + "/_metadata";
      std::ifstream metadata(metadata_path.c_str());
      if (!metadata.is_open()){
        UMAP_ERROR("Failed to open metadata file" << " - " << strerror(errno));
      }
      else{
        metadata.ignore ( std::numeric_limits<std::streamsize>::max(), '\n' );
        metadata >> capacity;
      }
      return capacity;
    }

    int SparseStore::get_fd(off_t offset, off_t &file_offset){
      int fd_index = offset / file_size;
      file_offset = offset % file_size; 
      std::string filename = root_path + "/" + std::to_string(fd_index);
      if ( file_descriptors[fd_index].id == -1 ){
            creation_mutex.lock(); // Grab mutex (only in case of creating new file, rather than serializing a larger protion of the code)
            if (file_descriptors[fd_index].id == -1){ // Recheck the value to make sure that another thread did not already create the file
              int flags = (read_only ? O_RDONLY :  O_RDWR ) | O_CREAT | O_DIRECT | O_LARGEFILE;
              int fd = open(filename.c_str(), flags, S_IRUSR | S_IWUSR);
              if (fd == -1){
                // Handling FS that do not support O_DIRECT, e.g., TMPFS
                flags = (read_only ? O_RDONLY :  O_RDWR ) | O_CREAT | O_LARGEFILE;
                fd = open(filename.c_str(), flags, S_IRUSR | S_IWUSR);
                if (fd == -1){
                   UMAP_ERROR("ERROR: Failed to open file with id: " << fd_index << " - " << strerror(errno));
                }
              }
              // when successfully open file:
              if (!read_only){
                int fallocate_status;
                if( (fallocate_status = posix_fallocate(fd,0,file_size) ) != 0){
                  UMAP_ERROR("SparseStore: fallocate() failed for file with id: " << fd_index << " - " << fallocate_status);
                }
              }
              // when fallocate() succeeds or when read_only
              file_descriptors[fd_index].id = fd;
            }
            creation_mutex.unlock(); // Release mutex
      }
      return file_descriptors[fd_index].id;
    }
}
