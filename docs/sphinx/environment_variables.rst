.. _environment_variables:

=============================
Runtime Environment Variables
=============================

The interface to the umap runtime library configuration is controlled by
the following environment variables.

.. code-block:: bash

    UMAP_PAGESIZE=$((2*4096)) your_program_that_uses_umap

The following environment varialbles may be set:

* ``UMAP_PAGE_FILLERS``
  This is the number of worker threads that will perform read operations from
  the backing store (including read-ahead) for a specific umap region.

  Default: `std::thread::hardware_concurrency()`

* ``UMAP_PAGE_EVICTORS``
  This is the number of worker threads that will perform evictions of pages.
  Eviction includes writing to the backing store if the page is dirty and
  telling the operating system that the page is no longer needed.
  
  Default: `std::thread::hardware_concurrency()`

* ``UMAP_EVICT_HIGH_WATER_THRESHOLD``
  This is an integer percentage of present pages in the Umap Buffer that
  informs the Eviction workers that it is time to start evicting pages.
  
  Default: 90

* ``UMAP_EVICT_LOW_WATER_THRESHOLD``
  This is an integer percentage of present pages in the Umap Buffer that
  informs the Eviction workers when to stop evicting.

  Default: 70

* ``UMAP_PAGESIZE``
  This is the size of the umap pages.  This must be a multiple of the system
  page size.

  Default: System Page Size

* ``UMAP_BUFSIZE``
  This is the total number of umap pages that may be present within the Umap
  Buffer.

  Default: (90% of free memory)

* ``UMAP_MONITOR_FREQ``
  This is the interval (in seconds) for the monitoring thread to print statistics, e.g., filled pages, 
  free pages and processed events for debugging or tuning.

  Default: 0
