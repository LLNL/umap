.. _caliper

==========================================
Integration within Caliper
==========================================

UMap can be integrated into the Caliper: A Performance Analysis Toolbox in a Library for providing page access pattern profiling.
We describe the steps for leveraging UMap in Caliper as follows:

* Install Caliper from https://github.com/LLNL/Caliper.git to <CALIPER_INSTALL_PATH>

* Build UMap by running cmake with -Dcaliper_DIR=<CALIPER_INSTALL_PATH>/share/cmake/caliper

* An example application is provided in /tests/caliper_trace. Make sure it is compiled

* Enable the page fault tracing before running the program to be profiled as follows

.. code:: bash

   export CALI_SERVICES_ENABLE=alloc,event,trace,recorder
   export CALI_ALLOC_TRACK_ALLOCATIONS=true
   export CALI_ALLOC_RESOLVE_ADDRESSES=true
   
* This should produce a .cali output file with an automatically generated filename, e.g., "200611-155825_69978_QSZC2zryxwRh.cali".


Now we describe how to analyze the profiling results. To simply print all records captured from Caliper, simply use:

.. code:: bash

   cali-query -t <filename> 


For advanced queries, e.g. count the number of page faults per memory region:

.. code:: bash

   cali-query -q "select alloc.label#pagefault.address,count() group by alloc.label#pagefault.address where pagefault.address format table" <filename> 

