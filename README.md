# UMAP v2.1.0

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

`LLNL-CODE-733797`

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

### Docker Container
There is a `Dockerfile` included in the repository to make it easier to build a container image with a basic Umap development environment.

A few caveats:
The [runtime requirements](#runtime-requirements) of the Docker container are the same as the non-containerized Umap. Additionally, it's important to note that Umap checks the kernel headers present in the build-time environment to decide whether or not WP mode should be enabled. The included `Dockerfile` will always build Umap with WP support enabled because the Ubuntu 22.04 kernel headers included in the the container indicate that WP is supported, even if the host kernel doesn't actually support that feature. Umap will return an error at runtime if it's built with WP enabled but run on a kernel without WP support.

Docker, as a security measure, uses a `seccomp` profile to restrict the syscalls allowed to be made by an application running inside a container to a minimal subset. Umap relies on being able to make host kernel syscalls that are otherwise blocked by Docker's default `seccomp` profile. Specifically, Umap relies on the `userfaultfd` syscall. See [here](https://docs.docker.com/engine/security/seccomp/#significant-syscalls-blocked-by-the-default-profile]) for more information about which syscalls are blocked by Docker's default `seccomp` profile.

#### Example: Building and running the Umap Docker container
```bash
docker build -t umap .
docker run --security-opt seccomp=unconfined -it umap bash
```

#### Example: Running the Umap container with a `seccomp` whitelist
It's also possible to run the container with a `seccomp` whitelist rather than disabling confinement entirely.
First, create a `userfaultfd.json` file:
```json
{"names": ["userfaultfd"], "action": "SCMP_ACT_ALLOW"}
```

When running the container:
```bash
docker run --security-opt seccomp=userfaultfd.json -it umap bash
```

## Build Requirements
Building Umap requires a C++ compiler, CMake >= 3.5.1, as well as the Linux kernel headers.

Additionally, Umap will automatically enable read/write mode *at library compile time* if it detects that the installed kernel supports it by looking at the defined symbols in the kernel headers. Some Linux distributions, such as Ubuntu 20.04.2, provide a 5.8 kernel that supports read/write mode but don't ship with headers that define these symbols.

Read-only mode: Linux kernel >= 4.3

Read/write mode: Linux kernel >= 5.7 (>= 5.10 preferred)

Note: Some early mainline releases of Linux that included support for read/write mode (between 5.7 and 5.9) contain a known bug that causes an application to hang indefinitely when performing a write. It's recommended to update to a 5.10 kernel if this bug is encountered.

## Runtime Requirements
At runtime, Umap requires a kernel that supports the same features as it was built against. For example, running a version of Umap compiled against kernel 5.10 on a system with kernel 4.18 will result in runtime errors caused by write functionality not being present in the 4.18 kernel.

On Linux >= 5.4, the `sysctl` variable `vm.unprivileged_userfaultfd` needs to be set to `1` in order to use Umap in both read-only and read/write modes as a non-root user. The value of this variable may be determined by running `sysctl vm.unprivileged_userfaultfd` or `cat /proc/sys/vm/unprivileged_userfaultfd`.

## Applications

Applications can be found at https://github.com/LLNL/umap-apps. 

## Documentation

Both user and code documentation is available
[here](http://llnl-umap.readthedocs.io/).

If you have build problems, we have comprehensive
[build sytem documentation](https://llnl-umap.readthedocs.io/en/develop/advanced_configuration.html) too!

## Publications

```Peng, I.B., Gokhale, M., Youssef, K., Iwabuchi, K. and Pearce, R., 2021. Enabling Scalable and Extensible Memory-mapped Datastores in Userspace. IEEE Transactions on Parallel and Distributed Systems. doi: 10.1109/TPDS.2021.3086302.```

```Peng, I.B., McFadden, M., Green, E., Iwabuchi, K., Wu, K., Li, D., Pearce, R. and Gokhale, M., 2019, November. UMap: Enabling application-driven optimizations for page management. In 2019 IEEE/ACM Workshop on Memory Centric High Performance Computing (MCHPC) (pp. 71-78). IEEE.```

## License

- The license is [LGPL](/LICENSE).
- [thirdparty_licenses.md](/thirdparty_licenses.md)

## Contact

- Ivy Peng  (ivybopeng@gmail.com)
- Maya Gokhale (gokhale2@llnl.gov)
- Eric Green (green77@llnl.gov)
