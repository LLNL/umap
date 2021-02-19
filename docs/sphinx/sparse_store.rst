.. _sparse_store

==========================================
Sparse Multi-files Backing Store Interface
==========================================

UMap provides a sparse and multi-files store object called "SparseStore", which partitions the backing file into multiple files that are created dynamically and only when needed. 

A SparseStore object is instantiated in either "create" or "open" mode.

In "create" mode, the total region size, page size, backing directory path, and partitioning granularity need to be specified.

In "open" mode, the backing directory path needs to be specified, along with a "read_only" boolean option. 

To instantiate and use a SparseStore object in "create" mode:

.. code-block:: c

     
     Umap::SparseStore * sparse_store;
     sparse_store = new Umap::SparseStore(numbytes,page_size,root_path,file_size);

     // set umap flags
     int flags = UMAP_PRIVATE;
     
     if (start_addr != nullptr)
      flags |= MAP_FIXED;

     const int prot = PROT_READ|PROT_WRITE;

     /* Map region using UMap, Here, the file descriptor passed to umap is -1, as we do not start with mapping a file
        instead, file(s) will be created incrementally as needed using the "SparseStore" object. */

     region = umap_ex(start_addr, numbytes, prot, flags, -1, 0, sparse_store);

To instantiate and use a SparseStore object in "open" mode:

.. code-block:: c

    Umap::SparseStore sparse_store;
    bool read_only = true;
    sparse_store = new Umap::SparseStore(root_path,read_only);

    // set umap flags
    int flags = UMAP_PRIVATE;

    if (start_addr != nullptr)
     flags |= MAP_FIXED;
    
    const int prot = PROT_READ|PROT_WRITE;
    
    region = umap_ex(start_addr, numbytes, prot, flags, -1, 0, sparse_store);
    

To unmap a region created with SparseStore, the SparseStore object needs to explicitely close the open files and then be deleted:

.. code-block:: c

    // Unmap region
    if (uunmap(region, numbytes) < 0) {
        // report failure and exit
    }

    int sparse_store_close_files = store->close_files();
    if (sparse_store_close_files != 0 ){
        // report failure and exit
    }
    delete store; 
   } 



