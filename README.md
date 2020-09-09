# UMAP v2.0.0

[![Travis Build Status](https://travis-ci.com/LLNL/umap.svg?branch=develop)](https://travis-ci.com/LLNL/umap)
[![Documentation Status](https://readthedocs.org/projects/llnl-umap/badge/?version=develop)](https://llnl-umap.readthedocs.io/en/develop/?badge=develop)

Umap is a library that provides an mmap()-like interface to a simple, user-
space page fault handler based on the userfaultfd Linux feature (starting with
4.3 linux kernel). The use case is to have an application specific buffer of
pages cached from a large file, i.e. out-of-core execution using memory map.

The src directory in the top level contains the source code for the library.

The tests directory contains various tests written to test the library
including a hello world program for userfaultfd based upon code from the
[userfaultfd-hello-world project](http://noahdesu.github.io/2016/10/10/userfaultfd-hello-world.html).

## Quick Start

*Building umap* is trivial. In the root directory of the repo

```bash
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=<where you want the sofware> ..
make install
```

The default for cmake is to build a Debug version of the software.  If you
would like to build an optimized (-O3) version, simply run 
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=<install-dir> ..
```

## Documentation

The design and implementation of UMap is described in the following paper:

```Peng, Ivy B., Marty McFadden, Eric Green, Keita Iwabuchi, Kai Wu, Dong Li, Roger Pearce, and Maya Gokhale. "UMap: Enabling Application-driven Optimizations for Page Management". In Proceedings of the Workshop on Memory Centric High Performance Computing. ACM, 2018.```

Both user and code documentation is available
[here](http://llnl-umap.readthedocs.io/).

If you have build problems, we have comprehensive
[build sytem documentation](https://llnl-umap.readthedocs.io/en/develop/advanced_configuration.html) too!

## License

- The license is [LGPL](/LICENSE).
- [thirdparty_licenses.md](/thirdparty_licenses.md)

`LLNL-CODE-733797`

## Contact

- Marty McFadden  (mcfadden8@llnl.gov)
- Maya Gokhale (gokhale2@llnl.gov)
- Eric Green (green77@llnl.gov)
