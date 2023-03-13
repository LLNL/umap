***********
UMAP v2.1.0
***********

Umap is a library that provides an mmap()-like interface to a simple, user-
space page fault handler based on the userfaultfd Linux feature (starting with
4.3 linux kernel). The use case is to have an application specific buffer of
pages cached from a large file, i.e. out-of-core execution using memory map.

- Take a look at our Getting Started guide for all you need to get up and
  running with umap.

- If you are looking for developer documentation on a particular function,
  check out the code documentation.

- Want to contribute? Take a look at our developer and contribution guides.

Any questions? File an issue on GitHub.

.. toctree::
  :maxdepth: 2
  :caption: Basics

  getting_started

.. toctree::
  :maxdepth: 2
  :caption: Reference

  advanced_configuration
  environment_variables
  sparse_store
  caliper
  
.. toctree::
  :maxdepth: 2
  :caption: Contributing

  contribution_guide
