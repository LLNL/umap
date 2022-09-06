UMAP_DIR=../../../..
PFM_DIR=/home/peng8/skylake/Software/perfmon2-libpfm4

CXX=g++
CXXFLAGS= -fopenmp -fPIC -O3 -Wall -std=c++14 -g -I../include -I${UMAP_DIR}/build/include

LDFLAGS= -lnuma -L../lib -lpebs \
		-lboost_system -lboost_thread \
		-L${PFM_DIR}/lib -lpfm \
		-L$(UMAP_DIR)/build/lib -lumap
EXE=main

default: $(EXE)

$(EXE): clean
	$(CXX) $(CXXFLAGS) -o $@  $(EXE).cc $(LDFLAGS)

run: $(EXE)
	sudo LD_LIBRARY_PATH=$(UMAP_DIR)/ext/profiling/pebs/lib:$(UMAP_DIR)/build/lib ./$(EXE)

clean:
	-rm -rf $(EXE) *~
