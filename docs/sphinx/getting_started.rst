.. _getting_started:

===============
Getting Started
===============

This page provides information on how to quickly get up and running with umap.

------------
Installation
------------

Umap is hosted on GitHub `here <https://github.com/LLNL/umap>`_.
To clone the repo into your local working space, type:

.. code-block:: bash

  $ git clone --recursive https://github.com/LLNL/umap.git

or

.. code-block:: bash

  $ git clone --recursive git@github.com:LLNL/umap.git

^^^^^^^^^^^^^^^
Building umap
^^^^^^^^^^^^^^^

Umap uses CMake to handle builds. Make sure that you have a modern
compiler loaded and the configuration is as simple as:

.. code-block:: bash

  $ mkdir build && cd build
  $ cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=my.install.dir ../

By default, umap will build a Release type build and will use the system
defined directories for installation.  You may specify a different build type
such as Debug and your own specific directory for installation by using the
two "-D" parameters above.  CMake will provide output about which compiler iscw
being used. Once CMake has completed, umap can be built with Make as follows:

.. code-block:: bash

  $ make

For more advanced configuration, see :doc:`advanced_configuration`.

^^^^^^^^^^^^^^^^^
Installing umap
^^^^^^^^^^^^^^^^^

To install umap, just run:

.. code-block:: bash

  $ make install

Umap install files to the ``lib``, ``include`` and ``bin`` directories of the
``CMAKE_INSTALL_PREFIX``. Additionally, umap installs a CMake configuration
file that can help you use umap in other projects. By setting `umap_DIR` to
point to the root of your umap installation, you can call
``find_package(umap)`` inside your CMake project and Umpire will be
automatically detected and available for use.

-----------
Basic Usage
-----------

Let's take a quick tour through Umap's most important features. A complete
listing you can compile is included at the bottom of the page. First, let's
grab an Allocator and allocate some memory. This is the interface through which
you will want to access data:

.. code-block:: cpp

  #include "umap.h"

  void* region = umap(NULL, 100*4096, PROT_READ|PROT_WRITE, UMAP_PRIVATE, fd, 0);

  float* my_data = static_cast<float*>region;

  my_data[0] = 3.1415;

  uunmap(region, 100*4096);

This code creates a 100*4096 byte mapping to the open file specified by the
``fd`` file descriptor.

