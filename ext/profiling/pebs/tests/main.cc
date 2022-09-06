#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <random>

#include "profiler.hh"
#include "umap/umap.h"

int main ()
{
    int fd = open("input128M.txt", O_RDWR | O_CREAT | O_DIRECT, S_IRUSR | S_IWUSR);
    if ( fd == -1 ) {
        int eno = errno;
        std::cerr << "Failed to open input file input128M.txt : " << strerror(eno) << std::endl;
        exit(1);
    }

    size_t nx = 16384;
    size_t ny = 1024;
    size_t len = sizeof(int) * nx * ny;
    void* ptr = umap(NULL, len, PROT_READ|PROT_WRITE, UMAP_PRIVATE, fd, 0);
    int* base_ptr = (int*) ptr;
    if ( ptr == UMAP_FAILED ) {
      int eno = errno;
      std::cerr << "Failed to umap " << len << " bytes : " << strerror(eno) << std::endl;
      exit(1);
    }

    Profiler* p = new Profiler();
    int **data = (int**) malloc (sizeof(int *) * ny);

    //Register ny memory regions in the profiler
    for (size_t k = 0; k < ny; k++)
    {
        data[k] = base_ptr;
        p->mt_register_address(data[k], sizeof(int)*nx);
        base_ptr += nx;
    }

    //Initialize the values in the umapped region
    for (size_t j = 0; j < ny; j++)
    {
        for (size_t i = 0; i < nx; i++)
        {
            data[j][i] = 1;
        }
    }

    //Init radom seed
    std::random_device rd;
    std::mt19937 gen(rd());

    //Select one random distribution
    std::poisson_distribution<> d(ny/2);
    //std::uniform_int_distribution<> d(0,ny-1);

    //Start the profiler
    p->start_all();

    //Start accessing the memory region following the random distribution
    int res = 0;
    for (size_t rep = 0; rep < 3000; rep++)
    {
        int j = d(gen);
        for (size_t i = 0; i < nx; i++)
        {
            res += data[j][i];
        }       
    }

    //Stop the profiler
    p->stop_all();

    //Print out the top 20 accessed memory regions
    p->view_topN(20);

    printf("RES=%d\n", res);//need this, otherwise compiler throw data access away

    delete p;
    free(data);
    //uunmap(ptr, len);
}
