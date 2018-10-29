# Usages

## Generate edge list using a R-MAT generator
cd rmat_edge_generator

./generate_edge_list

-o \[out edge list file name (required)\] \\\\

-s \[seed for random number generator; default is 123\]  \\\\

-v \[SCALE; The logarithm base two of the number of vertices; default is 17\]  \\\\

-e \[#of edges; default is 2^{SCALE} x 16\]  \\\\

-a \[Initiator parameter A; default is 0.57\]  \\\\

-b \[Initiator parameter B; default is 0.19\]  \\\\

-c \[Initiator parameter C; default is 0.19\]  \\\\

-r \[If true, scrambles edge IDs\; default is true]  \\\\

-u \[If true, generate edges for the both direction; default is true\]

* As for the initiator parameters,
see [Graph500, 3.2 Detailed Text Description](https://graph500.org/?page_id=12#sec-3_2) for more details.
* Our edge list ingest program read edge lists as directed graph.
If you use the ingest program, please specify 'true' to -u option (its default value is true).


## Ingest Edge List (construct CSR graph)

./ingest_edge_list -g /l/ssd/csr_graph_file /l/ssd/edgelist1 /l/ssd/edgelist2

* Load edge data from files /l/ssd/edgelist1 and /l/ssd/edgelist2 (you can specify arbitrary number of files).
* This is a multi-threads (OpenMP) program.
You can control the number of threads using the environment variable OMP_NUM_THREADS.
* Each line of the input files must be a pair of source and destination vertex IDs (unsigned 64bit number).
* The graph is constructed to /l/ssd/csr_graph_file
* As for real-world datasets, [SNAP Datasets](http://snap.stanford.edu/data/index.html) is a very popular in the graph processing community.
Please note that some datasets in SNAP are a little different.
For example, the first line is a comment; you have to delete the line before running this program.

## Run BFS
./run_bfs -n \[#of vertices\] -m \[#of edges\] -g /path/to/graph_file

* You can get #of vertices and #of edges by running ingest_edge_list
* This is a multi-threads (OpenMP) program.
You can control the number of threads using the environment variable OMP_NUM_THREADS.