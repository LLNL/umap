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

  auto& rm = umpire::ResourceManager::getInstance();
  umpire::Allocator allocator = rm.getAllocator("HOST");

  float* my_data = static_cast<float*>(allocator.allocate(100*sizeof(float));


This code grabs the default allocator for the host memory, and uses it to
allocate an array of 100 floats. We can ask for different Allocators to
allocate memory in different places. Let's ask for a device allocator:

.. code-block:: cpp

  umpire::Allocator device_allocator = rm.getAllocator("DEVICE");

  float* my_data_device = static_cast<float*>(device_allocator.allocate(100*sizeof(float));

This code gets the default device allocator, and uses it to allocate an array
of 100 floats. Remember, since this is a device pointer, there is no guarantee
you will be able to access it on the host.  Luckily, Umpire's ResourceManager
can copy one pointer to another transparently. Let's copy the data from our
first pointer to the DEVICE-allocated pointer.

.. code-block:: cpp

  rm.copy(my_data, my_data_device);

To free any memory allocated, you can use the deallocate function of the
Allocator, or the ResourceManager. Asking the ResourceManager to deallocate
memory is slower, but useful if you don't know how or where an allocation was
made:

.. code-block:: cpp

  allocator.deallocate(my_data); // deallocate using Allocator
  rm.deallocate(my_data_device); // deallocate using ResourceManager
