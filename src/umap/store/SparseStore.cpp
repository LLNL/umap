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

#ifdef USE_COMPRESSION
  #include <umap/util/Compression.hpp>
  #include <unistd.h>
#endif

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
      file_exists_map = new int8_t[num_files];
      for (int i = 0 ; i < num_files ; i++){
        file_descriptors[i].id = -1;
        file_exists_map[i] = 0;
      }
      DIR *directory;
      struct dirent *ent;
      std::string metadata_file_path = root_path + "/_metadata";
      if ((directory = opendir(root_path.c_str())) != NULL){
        UMAP_LOG(Warning, "Directory already exist... removing and creating a new directory"); // Needs to be opened in open mode: store = new SparseStore(root_path,is_read_only); ");
        std::string mkdir_command("rm -r " + root_path);
        const int status = std::system(mkdir_command.c_str());
        if (status == -1){
          UMAP_ERROR("Failed to remove directory" + root_path);
        }
      }
      //else{
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

      // File exists map
      std::string filemap_path = root_path + "/_filemap";
      filemap_fd = open(filemap_path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      if (filemap_fd == -1){
        UMAP_ERROR("SparseStore: failed to open filemap metadata - " << strerror(errno));
      }

      size_t written = pwrite(filemap_fd, (void*) file_exists_map, num_files * sizeof(int8_t), 0);
      if (written == -1){
        UMAP_ERROR("SparseStore: failed to write filemap metadata - " << strerror(errno));
      }

      // Zero page
      char *tmp;
      if (posix_memalign((void**)&tmp, ::umapcfg_get_umap_page_size(), ::umapcfg_get_umap_page_size())) {
        std::cerr << "Virtual Memory Manager: Error posix_memalign - " << strerror(errno) << std::endl;
      }
      zero_page = (char*) mmap((void *) tmp, ::umapcfg_get_umap_page_size(), PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_POPULATE, -1, 0);
      if (zero_page == MAP_FAILED){
        std::cerr << "Error mmap zero page " << strerror(errno) << std::endl;
        exit(-1);
      }
      
      // }
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
        file_exists_map = new int8_t[num_files];
        std::string filemap_path = root_path + "/_filemap";
        filemap_fd = open(filemap_path.c_str(), O_RDWR);
        if (filemap_fd == -1){
          UMAP_ERROR("SparseStore: failed to open filemap metadata - " << strerror(errno));
        }
        size_t read = pread(filemap_fd, (void*) file_exists_map, num_files *sizeof(int8_t), 0);
        if (read == -1){
          UMAP_ERROR("SparseStore: failed to read filemap metadata - " << strerror(errno));
        }
      }
      char *tmp;
      if (posix_memalign((void**)&tmp, ::umapcfg_get_umap_page_size(), ::umapcfg_get_umap_page_size())) {
        std::cerr << "Virtual Memory Manager: Error posix_memalign - " << strerror(errno) << std::endl;
      }
      zero_page = (char*) mmap((void *) tmp, ::umapcfg_get_umap_page_size(), PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_POPULATE, -1, 0);
      if (zero_page == MAP_FAILED){
        std::cerr << "Error mmap zero page " << strerror(errno) << std::endl;
        exit(-1);
      }
      closedir(directory);

    }
    

    SparseStore::~SparseStore(){
      UMAP_LOG(Info,"SparseStore Total Reads: " << numreads);
      UMAP_LOG(Info,"SparseStore Total Writes: " << numwrites); 
      delete [] file_descriptors;
      int status = munmap((void *) zero_page, file_size);
      if (status == -1){
        UMAP_ERROR("SparseStore: Error unmapping zero page - " << strerror(errno));
      }
      if (close(filemap_fd) == -1){
        UMAP_ERROR("SparseStore: failed to close filemap metadata - " << strerror(errno));
      }
      delete [] file_exists_map;
    }

    ssize_t SparseStore::read_from_store(char* buf, size_t nb, off_t off) {
      ssize_t read = 0;
      off_t file_offset;
      int fd_index = off / file_size;
      if (file_exists_map[fd_index] == 0){
        // return zero page
        memcpy(buf, (void*) zero_page, file_size);
        return file_size;
      }
      else{
        int fd = get_fd(off, file_offset, 0);
        #ifdef USE_COMPRESSION
          // Get compressed size using lseek
          size_t compressed_block_size = lseek(fd, 0, SEEK_END);
          if (compressed_block_size == -1){
            UMAP_ERROR("SparseStore: Failed to get file size, lseek failed with error - " << strerror(errno));
          }
          // Return to start of file
          int location = lseek(fd, 0, SEEK_SET);
          if (location == -1){
            UMAP_ERROR("SparseStore: Failed reset lseek with error - " << strerror(errno));
          }

          char* read_buffer;
          int memaligned_status = posix_memalign((void **)&read_buffer, ::umapcfg_get_umap_page_size(), compressed_block_size);
          if (memaligned_status != 0){
            UMAP_ERROR("SparseStore: Allocating temporary decompression buffer failed");
          }
          if (pread(fd, (void*)read_buffer, compressed_block_size, 0) == -1){
            UMAP_ERROR("pread(fd=" << fd << ", buff=" << (void*)buf <<  ", nb=" << nb << ", off=" << off << ") of compressed file Failed - " << strerror(errno));
          }
          size_t decompressed_size = Umap::decompress((void*)read_buffer, buf, compressed_block_size);
          free(read_buffer);
        #else
        read = pread(fd,buf,nb,file_offset);
        if(read == -1){
          UMAP_ERROR("pread(fd=" << fd << ", buff=" << (void*)buf <<  ", nb=" << nb << ", off=" << off << ") Failed - " << strerror(errno));
        }
        #endif
        numreads++;
        int close_status = close(fd);
        if (close_status == -1){
          UMAP_ERROR("Error Closing file descriptor for block " << (uint64_t) off << " - " << strerror(errno))
        }
        file_descriptors[fd_index].id = -1;
        return read;
      }
    }

    ssize_t SparseStore::write_to_store(char* buf, size_t nb, off_t off) {
      ssize_t written = 0;
      off_t file_offset;
      int fd_index = off / file_size;
      int fd = -1;
      #ifdef USE_COMPRESSION
        std::pair<void*,size_t> compressed_buffer_and_size = Umap::compress(buf, file_size);
        void* const write_buffer = compressed_buffer_and_size.first;
        size_t compressed_block_size = compressed_buffer_and_size.second;
        fd = get_fd(off, file_offset, compressed_block_size); 
        written = pwrite(fd, write_buffer, compressed_block_size, file_offset);
      #else
      fd = get_fd(off, file_offset, 0); 
      written = pwrite(fd,buf,nb,file_offset);
      #endif
      if(written == -1){
        UMAP_ERROR("pwrite(fd=" << fd << ", buff=" << (void*)buf <<  ", nb=" << nb << ", off=" << off << ") Failed - " << strerror(errno));
      }

      size_t written_filemap = pwrite(filemap_fd, (void*) file_exists_map, num_files * sizeof(int8_t), 0);
      if (written_filemap == -1){
        UMAP_ERROR("SparseStore: failed to write filemap metadata - " << strerror(errno));
      }
      numwrites++;
      int close_status = close(fd);
      if (close_status == -1){
        UMAP_ERROR("Error Closing file descriptor for block " << (uint64_t) off << " - " << strerror(errno))
      }
      file_descriptors[fd_index].id = -1;
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

    int SparseStore::get_fd(off_t offset, off_t &file_offset, size_t trunc_size){
      int fd_index = offset / file_size;
      file_offset = offset % file_size; 
      std::string filename = root_path + "/" + std::to_string(fd_index);
      if ( file_descriptors[fd_index].id == -1 ){
            creation_mutex.lock(); // Grab mutex (only in case of creating new file, rather than serializing a larger protion of the code)
            if (file_descriptors[fd_index].id == -1){ // Recheck the value to make sure that another thread did not already create the file
              int flags = (read_only ? O_RDONLY :  O_RDWR ) | O_CREAT | O_LARGEFILE;
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
                size_t fallocate_size = trunc_size != 0 ? trunc_size : file_size;
                if( (fallocate_status = posix_fallocate(fd,0,fallocate_size) ) != 0){
                  UMAP_ERROR("SparseStore: fallocate() failed for file with id: " << fd_index << " - " << fallocate_status);
                }
              }
              // when fallocate() succeeds or when read_only
              file_descriptors[fd_index].id = fd;
              file_exists_map[fd_index] = 1;
            }
            creation_mutex.unlock(); // Release mutex
      }
      return file_descriptors[fd_index].id;
    }
}
