## Generate edge list using a R-MAT generator
```
./rmat_edge_generator/generate_edge_list

-o [out edge list file name (required)]

-s [seed for random number generator; default is 123]

-v [SCALE; The logarithm base two of the number of vertices; default is 17]

-e [#of edges; default is 2^{SCALE} x 16]

-a [initiator parameter A; default is 0.57]

-b [initiator parameter B; default is 0.19]

-c [initiator parameter C; default is 0.19]

-r [if true, scrambles edge IDs; default is true]

-u [if true, generates edges for the both direction; default is true]
```

* As for the initiator parameters,
see [Graph500, 3.2 Detailed Text Description](https://graph500.org/?page_id=12#sec-3_2) for more details.

#### Generate Graph 500 inputs
```
./generate_edge_list -o /mnt/ssd/edge_list -v 20 -e $((2**20*16))
````
This command generates a edge list file (/mnt/ssd/edge_list) which contains the edges of a SCALE 20 graph.
In Graph 500, the number of edges of a graph is #vertives x 16 (16 is called 'edge factor').

## Ingest Edge List (construct CSR graph)
```
./ingest_edge_list -g /l/ssd/csr_graph_file /l/ssd/edgelist1 /l/ssd/edgelist2
```

* Load edge data from files /l/ssd/edgelist1 and /l/ssd/edgelist2 (you can specify an arbitrary number of files).
A CSR graph is constructed in /l/ssd/csr_graph_file.
* Each line of input files must be a pair of source and destination vertex IDs (unsigned 64bit number).
* This program treats inputs as a directed graph, that is, it does not ingest edges for both directions.
* This is a multi-threads (OpenMP) program.
You can control the number of threads using the environment variable OMP_NUM_THREADS.
* As for real-world datasets, [SNAP Datasets](http://snap.stanford.edu/data/index.html) is popular in the graph processing community.
Please note that some datasets in SNAP are a little different.
For example, the first line is a comment; you have to delete the line before running this program.

## Run BFS
```
./run_bfs -n \[#of vertices\] -m \[#of edges\] -g \[/path/to/graph_file\]
```

* You can get #of vertices and #of edges by running ingest_edge_list.
* This is a multi-threads (OpenMP) program.
You can control the number of threads using the environment variable OMP_NUM_THREADS.
