# UMAP v0.0.1 (alpha)

Umap is a library that provides an mmap()-like interface to a simple, user-
space page fault handler based on the userfaultfd Linux feature (starting with
4.3 linux kernel). The use case is to have an application specific buffer of
pages cached from a large file, i.e. out-of-core execution using memory map.

The src directory in the top level contains the source code for the library.

The tests directory contains various tests written to test the library
including a hello world program for userfaultfd based upon code from the
[userfaultfd-hello-world project](http://noahdesu.github.io/2016/10/10/userfaultfd-hello-world.html).


The sortbenchmark directory is the original sort benchmark, modified to use
threads rather than forking processes.

## Quick Start

*Building umap* is trivial. In the root directory of the repo

```bash
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX = <where you want the sofware> ..
make install
```

## umap() Interface

The interface is currently a work in progress (see [umap.h](src/umap.h)).

## Contact/Legal

The license is [LGPL](/LGPL).

Primary contact/Lead developer

- Maya Gokhale (gokhale2@llnl.gov)

Other developers

- Marty McFadden  (mcfadden8@llnl.gov)
- Xiao Liu  (liu61@llnl.gov)
