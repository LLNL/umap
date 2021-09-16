.. _advanced_configuration:

======================
Advanced Configuration
======================

Listed below are the MP-Umap-specific options which may be used when configuring
your build directory with cmake.  Some CMake-specific options have also been
added to show how to make additional changes to the build configuration.

.. code-block:: bash

    cmake -DENABLE_LOGGING=Off

Here is a summary of the configuration options, their default value, and meaning:

      ===========================  ======== ==========================================
      Variable                     Default  Meaning
      ===========================  ======== ==========================================
      ``ENABLE_LOGGING``           On       Enable Logging within mp-umap service
      ``ENABLE_DISPLAY_STATS``     Off      Enable Displaying mp-umap service stats on unmapping of a region 
      ``ENABLE_TESTS``             On       Enable building and installation of test applications
      ``ENABLE_TESTS_LINK_STATIC_UMAP``  Off      Generate tests statically linked with MP-Umap
      ``CMAKE_CXX_COMPILER``       not set  Specify C++ compiler to use
      ``DCMAKE_CC_COMPILER``       not set  Specify C compiler to use
      ===========================  ======== ==========================================

These arguments are explained in more detail below:

* ``ENABLE_LOGGING``
  This option enables usage of Logging for mp-umap service apps.  When this support is
  enabled, you may cause mpumapd library to emit log files by setting the ``UMAP_LOG_LEVEL``
  environment variable to "INFO" (for information-only logs), "WARNING" (for warning info
  logs), "ERROR" for (for errors only logs), and "DEBUG" for all debug messages to be emitted to a log file.

* ``ENABLE_DISPLAY_STATS``
  When this option is turned on, the mpumapd library will display its runtime
  statistics before client_uunmap() completes.

* ``ENABLE_TESTS``
  This option enables the compilation of the programs under the tests directory
  of the mp-umap source code.

* ``ENABLE_TESTS_LINK_STATIC_UMAP``
  This option enables the compilation of the programs under the tests directory
  of the mp-umap source code against static mp-umap libraries.
