# Run
## Ingest Edge List (construct CSR graph)

./ingest_edge_list -g /l/ssd/csr_graph_file /l/ssd/edgelist1 /l/ssd/edgelist2

* Load edge data from files /l/ssd/edgelist1 and /l/ssd/edgelist2 (you can specify arbitrary number of files)
* The graph is constructed to /l/ssd/csr_graph_file

## Run BFS
./run_bfs -n \[#of vertices\] -m \[#of edges\] -g /path/to/graph_file

* You can get #of vertices and #of edges by running ingest_edge_list