.. _getting_started:

===============
Getting Started
===============

This page provides information on how to quickly get up and running with umap.

^^^^^^^^^^^^^
Dependencies
^^^^^^^^^^^^^
At a minimum, cmake 3.5.1 or greater is required for building umap.

To enable the network-based datastore, the following external libraries are required.

  1. The Mercury RPC library (https://github.com/mercury-hpc/mercury.git)
  
  2. The Argobots threading framework (The https://github.com/pmodels/argobots.git)
  
  3. The Margo wrapper library (https://xgitlab.cels.anl.gov/sds/margo.git)

You can either install each of the above libraries manually, or run setup.sh to setup automatically.
Define UMAP_DEP_ROOT if you want to install the libraries in a custom path:  

.. code-block:: bash

  $ export UMAP_DEP_ROOT=<Place to install the Margo libraries>
  $ ./setup.sh
  
---------------------------
UMAP Build and Installation
---------------------------
Clone from the UMap git repository and set MARGO_ROOT to where you installed the dependency libraries:

.. code-block:: bash

  $ git clone https://github.com/LLNL/umap.git
  $ git checkout remote_region
  $ mkdir build && cd build
  $ cmake3 -DCMAKE_INSTALL_PREFIX="<Place to install umap>" -DMARGO_ROOT="$UMAP_DEP_ROOT" ..
  $ make
  $ make install

By default, umap will build a Release type build and will use the system
defined directories for installation.  To specify different build types or
specify alternate installation paths, see the :doc:`advanced_configuration`.

Umap install files to the ``lib``, ``include`` and ``bin`` directories of the
``CMAKE_INSTALL_PREFIX``.

-----------
Basic Usage
-----------

The interface to umap mirrors that of mmap(2) as shown:

.. literalinclude:: ../../examples/psort.cpp
                    :lines: 57-64

The following code is a simple example of how one may use umap:

.. literalinclude:: ../../examples/psort.cpp

-------------------
Network-based Usage
-------------------

The following code demonstrates how to create a network-based datastore on a server process:

.. code-block:: c

  Umap::Store* datastore  = new Umap::StoreNetworkServer("a", server_mem_addr, umap_region_length);
  
The following code demonstrates how to umap a network-based datastore on a client process:

.. code-block:: c

  void* base_addr   = umap_network("a", NULL, umap_region_length);  
  if ( base_addr == UMAP_FAILED ) {
    int eno = errno;
    std::cerr << "Failed to umap network" << ": " << strerror(eno) << std::endl;
    return 0;
  }
  
To run the application on a cluster, first start the server processes.

.. code-block:: console

    srun --ntasks-per-node=$numServerProcPerNode -N $numServerNodes ${EXE}_server & 
  
Then start the client processes after the server has published its connection information in serverfile.

.. code-block:: console

    srun --ntasks-per-node=$numClientProcPerNode -N $numClientNodes ${EXE}_client 
    
Tests of the network based handler can be find in <root>/tests/remote_xx folders    
