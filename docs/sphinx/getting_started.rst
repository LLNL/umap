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

The interface to umap mirrors that of mmap(2) as shown:

.. literalinclude:: ../../examples/psort.cpp
                    :lines: 29-33

The following code is a simple example of how one may use umap:

.. literalinclude:: ../../examples/psort.cpp
