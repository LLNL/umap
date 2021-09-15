.. _getting_started:

===============
Getting Started
===============

This page provides information on how to quickly get up and running with mp-umap.

^^^^^^^^^^^^^
Dependencies
^^^^^^^^^^^^^
At a minimum, cmake 3.5.1 or greater is required for building umap.

---------------------------
UMAP Build and Installation
---------------------------
MP-Umap provides service and client libraries. The former enables developers to launch mp-umap service that transparently manages 
the shared UMAP buffer and the  client API allows multi-processes to bind/interact with the service thereby enabling shared 
access to UMAP buffer. The following lines should get you up and running:

.. code-block:: bash

  $ git clone https://github.com/LLNL/umap.git
  $ git checkout mp_umap_pre_release
  $ cd umap
  $ git checkout mp_umap
  $ cmake3 -DCMAKE_BUILD_PREFIX=. -DENABLE_TESTS_LINK_STATIC_UMAP=ON -DCMAKE_INSTALL_PREFIX=../install ..; 
  $ cmake3 --build . --target install

MP-Umap is an experimental branch of Umap that enables sharing of file-backed Umap buffers
to multiple processes. At the moment, this feature is only available in read-only mode.
By default, mpumap builds with will build a Release type build and will use the system
defined directories for installation.  To specify different build types or
specify alternate installation paths, see the :doc:`advanced_configuration`.

Umap install files to the ``lib``, ``include`` and ``bin`` directories of the
``CMAKE_INSTALL_PREFIX``.

-----------
Basic Usage
-----------

MP-Umap library provides seperate interfaces for service and client processes.

Service communicate with the client processes through a Unix Domain socket.
This can be accomplished by including P-umap provides default option for UMAP_SERVER_PATH unless the user chooses
to provide a different location for the Unix socket to the following service interface.

.. literalinclude:: ../../tests/umap-service/umap-server.cpp
                    :lines: 8-23

The following code is a simple example of how one may use umap:

.. literalinclude:: ../../examples/psort.cpp
