/* $Id: flipx.c,v 1.10 2006/02/17 10:26:58 yjung Exp $
 *
 * This file is part of the ESO QFITS Library
 * Copyright (C) 2001-2004 European Southern Observatory
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * $Author: yjung $
 * $Date: 2006/02/17 10:26:58 $
 * $Revision: 1.10 $
 * $Name: qfits-6_2_0 $
 */

/*-----------------------------------------------------------------------------
                                   Includes
 -----------------------------------------------------------------------------*/
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>

#define NUMPAGES 10000
#define NUMTHREADS 1
#define BUFFERSIZE 100

#include "umap.h"
#include "umaptest.h"

extern "C"
{
#include "qfits.h"
}

umt_optstruct_t options;

static void copypix(char * buf,char * buf2, int p1, int psize)
{
    uint16_t *a1;
    uint32_t *a2;
    if (psize==2) 
    {
        a1=(uint16_t *)buf;
        printf("%u\n",*a1);
    }
    else
    {
        a2=(uint32_t *)buf;
        printf("%u\n",*a2);
    }
}
/*
 * Main processing function. It expects one only file name
 * and will flip pixels on the input frame.
 */
static int fits_flip(const char * filename)
{
    long pagesize;
    int64_t totalbytes;
    int fd;
    void *base_addr;
    struct stat        fileinfo ;

    pagesize = umt_getpagesize();
    if (stat(filename, &fileinfo)!=0) {
        return -1 ;
    }
    if (fileinfo.st_size<1) {
        printf("cannot stat file\n");
        return -1 ;
    }
    pagesize = umt_getpagesize();
    
    //totalbytes = options.numpages*pagesize;
    //printf("size:%d\n",fileinfo.st_size);
    fd=umt_openandmap(&options, options.numpages*pagesize, &base_addr);


    char        *    sval ;
    int                dstart;
    int                lx, ly ;
    int                bpp ;
    int                i, j ;
    char        *    buf ;
    char        *    buf2 ;
    char        *    fbuf ;
    int                psize;

    printf("processing %s\n",filename);

    /* Retrieve image attributes */
    if (qfits_is_fits(filename)!=1) {
        printf("not a FITS file\n");
        return -1 ;
    }

    sval = qfits_query_hdr(filename, "NAXIS1");
    if (sval==NULL) {
        printf("cannot read NAXIS1\n");
        return -1 ;
    }
    lx = atoi(sval);
    sval = qfits_query_hdr(filename, "NAXIS2");
    if (sval==NULL) {
        printf("cannot read NAXIS2\n");
        return -1 ;
    }
    ly = atoi(sval);
    sval = qfits_query_hdr(filename, "BITPIX");
    if (sval==NULL) {
        printf("cannot read BITPIX\n");
        return -1 ;
    }
    bpp = atoi(sval);

    psize = bpp/8 ;
    //printf("psize: %d uint32: %d\n",psize,sizeof(uint32_t));
    if (psize<0) psize=-psize ;

    /* Retrieve start of first data section */
    if (qfits_get_datinfo(filename, 0, &dstart, NULL)!=0) {
        printf("reading header information\n");
        return -1 ;
    }

    printf("psize:%d\n",psize);
    printf("dstart:%d\n",dstart);
    //Map the input file in read/write mode (input file is modified)
    /* if ((fd=open(filename, O_RDWR))==-1) { */
    /*     perror("open"); */
    /*     printf("reading file\n"); */
    /*     return -1 ; */
    /* } */
    /* fbuf = (char*)mmap(0, */
    /*                     fileinfo.st_size, */
    /*                     PROT_READ | PROT_WRITE, */
    /*                     MAP_SHARED, */
    /*                     fd , */
    /*                     0); */
    /* if (fbuf==(char*)-1) { */
    /*     perror("mmap"); */
    /*     printf("mapping file\n"); */
    /*     return -1 ; */
    /* } */

    //options.fn = filename;
    //fprintf(stdout, "USEFILE enabled %s\n", options.fn);
    // p->fd = open(options.fn, O_RDWR, S_IRUSR|S_IWUSR);// | O_DIRECT);
    // if (p->fd == -1) {
    //     perror("file open");
    //     exit(1);
    // }

    //printf("file opened!\n");
    // if ((fdnew=open("new.fits", O_RDWR))==-1)
    // {
    //     perror("open");
    //     printf("reading file\n");
    //     return -1 ;
    // }

    // if (stat("new.fits", &fileinfo2)!=0) {
    //     return -1 ;
    // }
    // fbuf2 = (char*)mmap(0,
    //                    fileinfo2.st_size,
    //                    PROT_READ | PROT_WRITE,
    //                    MAP_SHARED,
    //                    fdnew,
    //                    0);
    // if (fbuf2==(char*)-1) {
    //     perror("mmap");
    //     printf("mapping file\n");
    //     return -1 ;
    // }
    
    // buf2=fbuf2+dstart2;
    buf2=NULL;
    //pthread_create(&uffd_thread, NULL, uffd_handler, p);

    sleep(1);

    fbuf=(char *)base_addr;
    buf = fbuf + dstart ;
    //printf("%p\n");

    /* Double loop */
    //printf("lx ly:%d %d\n",lx,ly);
    /* for (i=0;i<fileinfo.st_size;i++) */
    /* { */
    /*     if (fbuf2[i]!=fbuf[i]) printf("here:%d\n",i); */
    /*     //printf("%c",fbuf[i]); */
    /*     //printf("%d\n",i); */
    /* } */
    for (j=0 ; j<ly ; j++) {
        for (i=0 ; i<lx ; i++) {
            /* Swap bytes */
            //swap_pix(buf,buf2, i*psize, (lx-i-1)*psize, psize);
            copypix(buf,buf2,i,psize);
        }
        printf("j:%d\n",j);
        buf  += lx * psize;
        //buf2 += lx * psize;
    }
    umt_closeandunmap(&options,options.numpages*pagesize,&base_addr,fd);
    return 0 ;
}

/*-----------------------------------------------------------------------------
                                Main
 -----------------------------------------------------------------------------*/
int main(int argc, char * argv[])
{
    int i ;
    int err ;
    umt_getoptions(&options, argc, argv);
    err=0;
    err += fits_flip(options.fn);
    if (err>0)
    {
        fprintf(stderr, "%s: %d error(s) occurred\n", argv[0], err);
        return -1 ;
    }
    return 0 ;
}
