# --- General configuration

# use Gauss-Seidel or Jacobi updates
JACOBI =
#JACOBI = -DJACOBI

# finite difference order
ORDER = 2
#ORDER = 4
#ORDER = 6
#ORDER = 8

# --- Number representation (set explicitly by targets below)

# number representation
REP = -DIEEE_PREC=64
#REP = -DIEEE_PREC=32
#REP = -DIEEE_PREC=16

#REP = -DPOSIT_PREC=64
#REP = -DPOSIT_PREC=32
#REP = -DPOSIT_PREC=16

#REP = -DZFP_RATE=32
#REP = -DZFP_RATE=16
#REP = -DZFP_TOLERANCE=1e-10

# --- Paths

ZFP_INC_DIR=/home/peng8/umap_multitier/umap_new/build_zfp/zfp/src/ZFP/include
ZFP_LIB_DIR=/home/peng8/umap_multitier/umap_new/build_zfp/zfp/src/ZFP-build
UMAP_DIR= /home/peng8/umap_multitier/umap_new/build_zfp

# --- Compiler and options
#ZFP_INC=-I${ZFP_DIR}/zfp/include -I${ZFP_DIR}/zfp/include/zfp -I${ZFP_DIR}/zfp/include/zfp/internal/array -I${ZFP_DIR}/zfp/include/zfp/codec
ZFP_INC=-I$(ZFP_INC_DIR) -I$(ZFP_INC_DIR)/zfp

CXX = g++
#CXXFLAGS = -ansi -pedantic -Wall -Wextra -O3 -I$(NUMREP_DIR) -I$(ZFP_DIR)/include -I$(ZFP_DIR)/array -DORDER=$(ORDER) $(JACOBI)
CXXFLAGS = -fopenmp -std=c++11 -Wall -Wextra -O3 -I$(UMAP_DIR)/include ${ZFP_INC} -DORDER=$(ORDER) $(JACOBI) 
LIBS = -L$(UMAP_DIR)/lib -lumap  -L$(ZFP_LIB_DIR)/lib -lzfp


# --- Targets

TARGETS = poisson-double poisson-float poisson-half \
	  poisson-posit32 poisson-posit16 \
	  poisson-zfp32 poisson-zfp16

all: poisson-double poisson-zfp16-original poisson-zfp16-umap  #$(TARGETS)

poisson: poisson.cpp
	$(CXX) $(CXXFLAGS) $(REP) $< -o $@

poisson-double: poisson.cpp
	$(CXX) $(CXXFLAGS) -DIEEE_PREC=64 $< -o $@ $(LIBS)

poisson-float: poisson.cpp
	$(CXX) $(CXXFLAGS) -DIEEE_PREC=32 $< -o $@ $(LIBS)

poisson-half: poisson.cpp
	$(CXX) $(CXXFLAGS) -DIEEE_PREC=16 $< -o $@ $(LIBS)

poisson-posit32: poisson.cpp
	$(CXX) $(CXXFLAGS) -DPOSIT_PREC=32 $< -o $@ $(LIBS)

poisson-posit16: poisson.cpp
	$(CXX) $(CXXFLAGS) -DPOSIT_PREC=16 $< -o $@ $(LIBS)

poisson-zfp32: poisson.cpp
	$(CXX) $(CXXFLAGS) -DZFP_RATE=32 $< -o $@ $(LIBS)

poisson-zfp16-original: poisson.cpp
	$(CXX) $(CXXFLAGS) -DZFP_RATE=16 $< -o $@ $(LIBS)

poisson-zfp16-umap: poisson.cpp
	$(CXX) $(CXXFLAGS) -DZFP_RATE=16 -DUSE_UMAP $< -o $@ $(LIBS)

clean:
	rm -f $(TARGETS) *.o *~ poisson-zfp16-original poisson-zfp16-umap
