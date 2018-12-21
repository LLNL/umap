.. _advanced_configuration:

======================
Advanced Configuration
======================

Listed below are the umap-specific options which may be used when configuring
your build directory with cmake.  Some CMake-specific options have also been
added to show how to make additional changes to the build configuration.

.. code-block:: bash

    cmake -DENABLE_LOGGING=Off

Here is a summary of the configuration options, their default value, and meaning:

      ===========================  ======== ==========================================
      Variable                     Default  Meaning
      ===========================  ======== ==========================================
      ``ENABLE_LOGGING``           On       Enable Logging within umap
      ``ENABLE_TESTS``             On       Enable building and installation of tests
      ``CMAKE_CXX_COMPILER``       not set  Specify C++ compiler to use
      ``DCMAKE_CC_COMPILER``       not set  Specify C compiler to use
      ===========================  ======== ==========================================

These arguments are explained in more detail below:

* ``ENABLE_LOGGING``
  This option enables usage of Logging services for umap.  When this support is
  enabled, you may cause umap library to emit log files by setting the ``UMAP_LOGGING``
  environment variable to "1" (for information-only logs), "2" (for more verbose
  logs), and "3" for all debug messages to be emitted to a log file.

* ``ENABLE_TESTS``
  This option enables the compilation of the programs under the tests directory
  of the umap source code.

