.. _sparse_store

==========================================
Sparse Multi-files Backing Store Interface
==========================================

UMap provides an extensible design that supports multiple types of backing stores (e.g., local SSDs, network-interconnected SSDs, and HDDs). 

An application that uses UMap can extend the abstract "Store" class to implement its specific backing store interface.

The default store object used by UMap is "StoreFile", which reads and writes to a single regular Linux file.

UMap also provides a sparse multi-files store object called "SparseStore", which creates multiple backing files dynamically and only when needed. 

Below is an example of using UMap with a SparseStore object.

.. code-block:: c

    #include <umap/umap.h>
    #include <umap/store/SparseStore.h>
    #include <string>
    #include <iostream>

    void use_sparse_store(
      std::string root_path,
      uint64_t numbytes,
      void* start_addr,
      size_t page_size,
      size_t file_size){

     void * region = NULL;
     
     // Instantiating a SparseStore objct, the "file_size" parameter specifies the granularity of each file. 
     // An application that desires to create N files can calculate the file size using totalbytes / num_files
     // Note that the file size is rounded to be a multiple of the page size, 
     // which results in a number of files that close but not exactly equal to N
     Umap::SparseStore* sparse_store;
     sparse_store = new Umap::SparseStore(numbytes,page_size,root_path,file_size);

     // Check status to make sure that the store object was able to open the directory
     if (store->get_directory_creation_status() != 0){
       std::cerr << "Error: Failed to create directory at " << root_path << std::endl;
       return NULL;
     }

     // set umap flags
     int flags = UMAP_PRIVATE;
     
     if (start_addr != nullptr)
      flags |= MAP_FIXED;

     const int prot = PROT_READ|PROT_WRITE;

     /* Map region using UMap, Here, the file descriptor passed to umap is -1, as we do not start with mapping a file
        instead, file(s) will be created incrementally as needed using the "sparse_store" object. */

     region = umap_ex(start_addr, numbytes, prot, flags, -1, 0, sparse_store);
     if ( region == UMAP_FAILED ) {
       std::ostringstream ss;
       ss << "umap_mf of " << numbytes
          << " bytes failed for " << root_path << ": ";
       perror(ss.str().c_str());
       exit(-1);
     }

     /*
      * some code that uses mapped region goes here. 
      */

    // Unmap region
    if (uunmap(region, numbytes) < 0) {
      std::ostringstream ss;
      ss << "uunmap of failure: ";
      perror(ss.str().c_str());
      exit(-1);
    }
    // NOTE: the method "close_files" from SparseStore MUST be called explicitely before deleting the object
    int sparse_store_close_files = store->close_files();
    if (sparse_store_close_files != 0 ){
      std::cerr << "Error closing SparseStore files" << std::endl;
      delete store;
      exit(-1);
    }
    delete store; 
   } 



