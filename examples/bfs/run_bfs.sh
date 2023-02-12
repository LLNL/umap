#!/bin/bash

##################################################
# This test script does not require sudo privilege 
# The tests run BFS, UMapsort, and Churn tests
# with different parameters
##################################################

if [ -z "$UMAP_ROOT" ];
then
    echo "UMAP_ROOT is not set."
    exit
fi

if [ -z "$UMAP_INSTALL_PATH" ];
then
    echo "UMAP_INSTALL_PATH is not set. use $UMAP_ROOT/build. "
    UMAP_INSTALL_PATH=$UMAP_ROOT/build
fi

INPUT_GRAPH="./test_graph"

export LD_LIBRARY_PATH=$UMAP_INSTALL_PATH/lib:$LD_LIBRARY_PATH

echo "##############################################"
echo "# BFS "
echo "##############################################"

./rmat_graph_generator/ingest_edge_list -g $INPUT_GRAPH $UMAP_ROOT/examples/bfs/data/edge_list_rmat_s10_?_of_4
echo ""
echo ""

./bfs -n 1017 -m 32768 -g $INPUT_GRAPH -l $UMAP_ROOT/examples/bfs/data//bfs_level_reference
/bin/rm -f $INPUT_GRAPH
echo ""
echo "BFS finished"
exit


