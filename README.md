UMap: User-space DI-MMAP)

# README #

This repo contains a simple page fault handler based on the userfaultfd Linux feature (starting with 4.3 kernel). The use case is to have an application specific buffer of pages cached from a large file, i.e. out-of-core execution using memory map. An example sort program that uses the handler interface has also been written.

The hello directory is the initial experiment, using code from noahdesu.github.io/2016/10/10/userfaultfd-hello-world.html.

The sortbenchmark directory is the original sort benchmark, modified to use threads rather than forking processes.

The uffd_handler directory contains the handler code. A simple test program uffd_test.c is provided. It can be compiled with or without a backing file. To use a backing file,
make D=USEFILE

If the kernel rev doesn't contain userfaultfd write protect protocol, compile with 
D=NOWP

Multiple defines are comma-separated: D=USEFILE,NOWP

Options to run the program include the number of openmp threads, total number of pages, the number of pages to buffer, and the file name.

The uffd_sort directory contains the sort benchmark modified to use the uffd_handler. To make it, type make. It always uses the backing file. It has the same set of runtime options as uffd_test:
the number of openmp threads, total number of pages, the number of pages to buffer, and the file name. Use the program od to look at the binary file after the program completes.