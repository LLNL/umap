.. _getting_started:

===============
Getting Started
===============

This page provides information on how to quickly get up and running with umap.

^^^^^^^^^^^^^
Dependencies
^^^^^^^^^^^^^
At a minimum, cmake 3.5.1 or greater is required for building umap.

---------------------------
UMAP Build and Installation
---------------------------
The following lines should get you up and running:

.. code-block:: bash

  $ git clone https://github.com/LLNL/umap.git
  $ mkdir build && cd build
  $ cmake -DCMAKE_INSTALL_PREFIX="<Place to install umap>" ../umap
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
                    :lines: 29-33

The following code is a simple example of how one may use umap:

.. literalinclude:: ../../examples/psort.cpp
