Testing features of the userfaultfd API to evaluate as basis for
dimmap2 

# README #

This repo contains a simple page fault handler based on the userfaultfd Linux feature (starting with 4.3 kernel). The use case is to have an application specific buffer of pages cached from a large file, i.e. out-of-core execution using memory map. An example sort program that uses the handler interface has also been written.