// Example for RDMA-based stream 

# include <stdio.h>
# include <unistd.h>
# include <math.h>
# include <float.h>
# include <limits.h>
# include <string.h>
# include <fcntl.h>
# include "errno.h"
# include <sys/time.h>
# include "umap/umap.h"
# include "umap/store/StoreNetwork.h"
#include <stdlib.h>
#include <omp.h>

#define STREAM_ARRAY_SIZE	1
#define NTIMES	3

# define HLINE "-------------------------------------------------------------\n"

# ifndef MIN
# define MIN(x,y) ((x)<(y)?(x):(y))
# endif
# ifndef MAX
# define MAX(x,y) ((x)>(y)?(x):(y))
# endif

#define STREAM_TYPE double


static STREAM_TYPE *a, *b, *c;

static double avgtime[4] = {0}, maxtime[4] = {0}, mintime[4] = {FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX};

static char	*label[4] = {"Copy:      ", "Scale:     ", "Add:       ", "Triad:     "};

static double bytes[4] = { 0, 0, 0, 0};

extern double mysecond();
extern void checkSTREAMresults();

int is_server;
char *server_name;
size_t array_size;


int main(int argc, char *argv[])
{
	is_server = 0;
	if(argc>1) is_server = atoi(argv[1]);

	// If Server
	if(is_server){
		server_name = NULL;
		Umap::NetworkServer server;
		server.wait_till_disconnect();
		server.close_ib_connection();
		return 0;
	}
	
	// If a Client
	server_name = strdup(argv[2]);
	Umap::NetworkClient *client = new Umap::NetworkClient(server_name);

	array_size = (size_t) STREAM_ARRAY_SIZE;
	if(argc>3) array_size = atol(argv[3]);

	size_t umap_region_length = sizeof(STREAM_TYPE) * array_size;
	size_t umap_psize = umapcfg_get_umap_page_size();
	umap_region_length = (umap_region_length + umap_psize - 1)/umap_psize * umap_psize;

	Umap::Store*  store_a = new Umap::StoreNetwork("stream_a", umap_region_length, umap_psize, client);
	a = (STREAM_TYPE*) umap_ex(NULL, umap_region_length, PROT_READ|PROT_WRITE, UMAP_PRIVATE, 0, 0, store_a);
	if ( a == UMAP_FAILED ) {
		printf("failed to map a %s \n", strerror(errno));
	}
	printf("Global umap psize = %zu, each array has %ld elements and %zu bytes\n", umap_psize, array_size, umap_region_length);
	a[0]=1234.5678;
#if false
	bytes[0] = 2 * sizeof(STREAM_TYPE) * array_size;
	bytes[1] = 2 * sizeof(STREAM_TYPE) * array_size;
	bytes[2] = 3 * sizeof(STREAM_TYPE) * array_size;
	bytes[3] = 3 * sizeof(STREAM_TYPE) * array_size;

    int			quantum, checktick();
    int			BytesPerWord;
    int			k;
    ssize_t		j;
    STREAM_TYPE		scalar;
    double		t, times[4][NTIMES];

    /* --- SETUP --- determine precision and check timing --- */
    printf(HLINE);
    printf("STREAM version $Revision: 5.10 $\n");
    printf(HLINE);
    BytesPerWord = sizeof(STREAM_TYPE);
    printf("This system uses %d bytes per array element.\n",
	BytesPerWord);
    printf(HLINE);

    printf("Array size = %llu (elements), Offset = %d (elements)\n" , (unsigned long long) array_size, 0);
    printf("Memory per array = %.1f MiB (= %.1f GiB).\n", 
	BytesPerWord * ( (double) array_size / 1024.0/1024.0),
	BytesPerWord * ( (double) array_size / 1024.0/1024.0/1024.0));
    printf("Total memory required = %.1f MiB (= %.1f GiB).\n",
	(3.0 * BytesPerWord) * ( (double) array_size / 1024.0/1024.),
	(3.0 * BytesPerWord) * ( (double) array_size / 1024.0/1024./1024.));
    printf("Each kernel will be executed %d times.\n", NTIMES);
    printf(" The *best* time for each kernel (excluding the first iteration)\n"); 
    printf(" will be used to compute the reported bandwidth.\n");

#ifdef _OPENMP
    printf(HLINE);
#pragma omp parallel 
    {
#pragma omp master
	{
	    k = omp_get_num_threads();
	    printf ("Number of Threads requested = %i\n",k);
        }
    }
#endif

#ifdef _OPENMP
	k = 0;
#pragma omp parallel
#pragma omp atomic 
		k++;
    printf ("Number of Threads counted = %i\n",k);
#endif

    /* Get initial value for system clock. */
	double t00 =  mysecond();
	#pragma omp parallel for
	for (j=0; j<array_size; j++) {
		a[j] = 1.0;
		b[j] = 2.0;
		c[j] = 0.0;
	}
	double t11 =  mysecond();
	printf("TIME IN SECONDS for initialization is %.3f\n", (t11-t00));//return 0;
	printf(HLINE);

	if  ( (quantum = checktick()) >= 1) 
		printf("Your clock granularity/precision appears to be "
			"%d microseconds.\n", quantum);
	else {
		printf("Your clock granularity appears to be "
			"less than one microsecond.\n");
		quantum = 1;
	}
	printf("Init is done\n");
		
	/*	--- MAIN LOOP --- repeat test cases NTIMES times --- */
	scalar = 3.0;
	for (k=0; k<NTIMES; k++)
	{
		times[0][k] = mysecond();
#pragma omp parallel for
		for (j=0; j<array_size; j++)
			c[j] = a[j];
		times[0][k] = mysecond() - times[0][k];
	
		times[1][k] = mysecond();
#pragma omp parallel for
		for (j=0; j<array_size; j++)
			b[j] = scalar*c[j];
		times[1][k] = mysecond() - times[1][k];
	
		times[2][k] = mysecond();
#pragma omp parallel for
		for (j=0; j<array_size; j++)
			c[j] = a[j]+b[j];
		times[2][k] = mysecond() - times[2][k];
	
		times[3][k] = mysecond();
#pragma omp parallel for
		for (j=0; j<array_size; j++)
			a[j] = b[j]+scalar*c[j];
		times[3][k] = mysecond() - times[3][k];
	}

	/*	--- SUMMARY --- */

	for (k=1; k<NTIMES; k++) /* note -- skip first iteration */
	{
		for (j=0; j<4; j++)
		{
			avgtime[j] = avgtime[j] + times[j][k];
			mintime[j] = MIN(mintime[j], times[j][k]);
			maxtime[j] = MAX(maxtime[j], times[j][k]);
		}
	}
	
	printf("Function    Best Rate MB/s  Avg time     Min time     Max time\n");
	for (j=0; j<4; j++) {
		avgtime[j] = avgtime[j]/(double)(NTIMES-1);

		printf("%s%12.1f  %11.6f  %11.6f  %11.6f\n", label[j],
		1.0E-06 * bytes[j]/mintime[j],
		avgtime[j],
		mintime[j],
		maxtime[j]);
	}
	printf(HLINE);

	/* --- Check Results --- */
	checkSTREAMresults();
	printf(HLINE);
#endif

	/* --- Free up --- */
	uunmap(a,0);
	uunmap(b,0);
	uunmap(c,0);
	
	client->close_ib_connection();
	free(client);
    return 0;
}

# define	M	20

int checktick()
{
    int		i, minDelta, Delta;
    double	t1, t2, timesfound[M];

/*  Collect a sequence of M unique time values from the system. */

    for (i = 0; i < M; i++) {
		t1 = mysecond();
		while( ((t2=mysecond()) - t1) < 1.0E-6 )
	    ;
		timesfound[i] = t1 = t2;
	}

/*
 * Determine the minimum difference between these M values.
 * This result will be our estimate (in microseconds) for the
 * clock granularity.
 */

    minDelta = 1000000;
    for (i = 1; i < M; i++) {
	Delta = (int)( 1.0E6 * (timesfound[i]-timesfound[i-1]));
	minDelta = MIN(minDelta, MAX(Delta,0));
	}

   return(minDelta);
}



/* A gettimeofday routine to give access to the wall
   clock timer on most UNIX-like systems.  */

#include <sys/time.h>

double mysecond()
{
        struct timeval tp;
        struct timezone tzp;
        int i;

        i = gettimeofday(&tp,&tzp);
        return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
}

/* 
   create a file of a defined size
 */

int open_prealloc_file( const char* fname, size_t totalbytes, int is_init)
{
	if(is_init){
		// delete the file
		int status = unlink(fname);
		int eno = errno;
		if ( status!=0 && eno != ENOENT ) {
			strerror(eno);
			printf("Failed to unlink %s: Errno=%d\n", fname, eno);
			return -1;
		}
	}

	int fd = open(fname, O_RDWR | O_CREAT | O_DIRECT, S_IRUSR | S_IWUSR);
	if ( fd == -1 ) {
		int eno = errno;
		printf("Failed to create %s: %s Errno=%d\n", fname, strerror(eno), eno);
		return -1;
	}

	if(is_init){
		// Pre-allocate disk space for the file.
		int x;
		if ( ( x = posix_fallocate(fd, 0, totalbytes) != 0 ) ) {
			int eno = errno;
			printf("Failed to pre-allocate %s: %s Errno=%d\n", fname, strerror(eno), eno);
			return -1;
		}
	}

  return fd;
}

#ifndef abs
#define abs(a) ((a) >= 0 ? (a) : -(a))
#endif
void checkSTREAMresults ()
{
	STREAM_TYPE aj,bj,cj,scalar;
	STREAM_TYPE aSumErr,bSumErr,cSumErr;
	STREAM_TYPE aAvgErr,bAvgErr,cAvgErr;
	double epsilon;
	ssize_t	j;
	int	k,ierr,err;

    /* reproduce initialization */
	aj = 1.0;
	bj = 2.0;
	cj = 0.0;
    /* a[] is modified during timing check */
	aj = 2.0E0 * aj;
    /* now execute timing loop */
	scalar = 3.0;
	for (k=0; k<NTIMES; k++)
        {
            cj = aj;
            bj = scalar*cj;
            cj = aj+bj;
            aj = bj+scalar*cj;
        }

    /* accumulate deltas between observed and expected results */
	aSumErr = 0.0;
	bSumErr = 0.0;
	cSumErr = 0.0;
	for (j=0; j<array_size; j++) {
		aSumErr += abs(a[j] - aj);
		bSumErr += abs(b[j] - bj);
		cSumErr += abs(c[j] - cj);
		// if (j == 417) printf("Index 417: c[j]: %f, cj: %f\n",c[j],cj);	// MCCALPIN
	}
	aAvgErr = aSumErr / (STREAM_TYPE) array_size;
	bAvgErr = bSumErr / (STREAM_TYPE) array_size;
	cAvgErr = cSumErr / (STREAM_TYPE) array_size;

	if (sizeof(STREAM_TYPE) == 4) {
		epsilon = 1.e-6;
	}
	else if (sizeof(STREAM_TYPE) == 8) {
		epsilon = 1.e-13;
	}
	else {
		printf("WEIRD: sizeof(STREAM_TYPE) = %lu\n",sizeof(STREAM_TYPE));
		epsilon = 1.e-6;
	}

	err = 0;
	if (abs(aAvgErr/aj) > epsilon) {
		err++;
		printf ("Failed Validation on array a[], AvgRelAbsErr > epsilon (%e)\n",epsilon);
		printf ("     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",aj,aAvgErr,abs(aAvgErr)/aj);
		ierr = 0;
		for (j=0; j<array_size; j++) {
			if (abs(a[j]/aj-1.0) > epsilon) {
				ierr++;
//#ifdef VERBOSE
				if (ierr < 10) {
					printf("         array a: index: %ld, expected: %e, observed: %e, relative error: %e\n",
						j,aj,a[j],abs((aj-a[j])/aAvgErr));
				}
//#endif
			}
		}
		printf("     For array a[], %d errors were found.\n",ierr);
	}
	if (abs(bAvgErr/bj) > epsilon) {
		err++;
		printf ("Failed Validation on array b[], AvgRelAbsErr > epsilon (%e)\n",epsilon);
		printf ("     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",bj,bAvgErr,abs(bAvgErr)/bj);
		printf ("     AvgRelAbsErr > Epsilon (%e)\n",epsilon);
		ierr = 0;
		for (j=0; j<array_size; j++) {
			if (abs(b[j]/bj-1.0) > epsilon) {
				ierr++;
#ifdef VERBOSE
				if (ierr < 10) {
					printf("         array b: index: %ld, expected: %e, observed: %e, relative error: %e\n",
						j,bj,b[j],abs((bj-b[j])/bAvgErr));
				}
#endif
			}
		}
		printf("     For array b[], %d errors were found.\n",ierr);
	}
	if (abs(cAvgErr/cj) > epsilon) {
		err++;
		printf ("Failed Validation on array c[], AvgRelAbsErr > epsilon (%e)\n",epsilon);
		printf ("     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",cj,cAvgErr,abs(cAvgErr)/cj);
		printf ("     AvgRelAbsErr > Epsilon (%e)\n",epsilon);
		ierr = 0;
		for (j=0; j<array_size; j++) {
			if (abs(c[j]/cj-1.0) > epsilon) {
				ierr++;
#ifdef VERBOSE
				if (ierr < 10) {
					printf("         array c: index: %ld, expected: %e, observed: %e, relative error: %e\n",
						j,cj,c[j],abs((cj-c[j])/cAvgErr));
				}
#endif
			}
		}
		printf("     For array c[], %d errors were found.\n",ierr);
	}
	if (err == 0) {
		printf ("Solution Validates: avg error less than %e on all three arrays\n",epsilon);
	}
#ifdef VERBOSE
	printf ("Results Validation Verbose Results: \n");
	printf ("    Expected a(1), b(1), c(1): %f %f %f \n",aj,bj,cj);
	printf ("    Observed a(1), b(1), c(1): %f %f %f \n",a[1],b[1],c[1]);
	printf ("    Rel Errors on a, b, c:     %e %e %e \n",abs(aAvgErr/aj),abs(bAvgErr/bj),abs(cAvgErr/cj));
#endif
}


