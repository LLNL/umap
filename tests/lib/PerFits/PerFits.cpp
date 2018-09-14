/*
 * This file is part of UMAP.
 *
 * For copyright information see the COPYRIGHT file in the top level
 * directory or at https://github.com/LLNL/umap/blob/master/COPYRIGHT.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License (as published by
 * the Free Software Foundation) version 2.1 dated February 1999.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA
 */
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <cassert>
#include <unordered_map>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "umap.h"
#include "testoptions.h"
#include "Tile.hpp"
#include "PerFits.h"
#include "spindle_debug.h"

using namespace std;

namespace PerFits {

struct Cube {
  const umt_optstruct_t test_options;
  size_t tile_size;  // Size of each tile (assumed to be the same for each tile)
  size_t cube_size;  // Total bytes in cube
  off_t page_size;
  vector<Fits::Tile> tiles;  // Just one column for now
};

static std::unordered_map<void*, Cube*> Cubes;

static ssize_t ps_read(void* region, char* dblbuf, size_t nbytes, off_t region_offset)
{
  auto it = Cubes.find(region);
  assert( "ps_read: failed to find control object" && it != Cubes.end() );
  Cube* cube = it->second;
  ssize_t rval;
  ssize_t bytesread = 0;
  char* direct_io_buf = &dblbuf[nbytes];

  //
  // Now read in remaining bytes
  //
  while ( nbytes ) {
    off_t tileno = region_offset / cube->tile_size;
    off_t tileoffset = region_offset % cube->tile_size;
    size_t bytes_to_eof = cube->tile_size - tileoffset;
    size_t bytes_to_read = std::min(bytes_to_eof, nbytes);

    debug_printf("dblbuf=%p, bytes_to_read=%zu, offset=%zu\n", dblbuf, bytes_to_read, tileoffset);
    if ( ( rval = cube->tiles[tileno].pread(cube->page_size, direct_io_buf, dblbuf, bytes_to_read, tileoffset) ) == -1) {
      perror("ERROR: pread failed");
      exit(1);
    }

    bytesread += rval;
    nbytes -= rval;
    dblbuf += rval;
    region_offset += rval;
  }

  return bytesread;
}

static ssize_t ps_write(void* region, char* buf, size_t nbytes, off_t region_offset)
{
  assert("FITS write not supported" && 0);
  return 0;
}

void* PerFits_alloc_cube(
    const umt_optstruct_t* TestOptions, /* Input */
    size_t* BytesPerElement,            /* Output: size of each element of cube */
    size_t* xDim,                       /* Output: Dimension of X */
    size_t* yDim,                       /* Output: Dimension of Y */
    size_t* zDim                        /* Output: Dimension of Z */
)
{
  void* region = NULL;

  if ( TestOptions->usemmap ) {
    cerr << "MMAP is not supported for FITS files\n";
    return nullptr;
  }

  if ( TestOptions->initonly ) {
    cerr << "INIT/Creation of FITS files is not supported\n";
    return nullptr;
  }

  Cube* cube = new Cube{.test_options = *TestOptions, .tile_size = 0, .cube_size = 0};
  cube->page_size = umt_getpagesize();
  string basename(TestOptions->filename);

  *xDim = *yDim = *BytesPerElement = 0;
  for (int i = 1; ; ++i) {
    stringstream ss;
    //ss << basename << std::setfill('0') << std::setw(3) << i << ".fits";
    ss << basename << i << ".fits";
    struct stat sbuf;

    if ( stat(ss.str().c_str(), &sbuf) == -1 ) {
      if ( i == 1 ) {
        cerr << "File: " << ss.str() << " does not exist\n";
        return region;
      }
      break;
    }

    Fits::Tile T( ss.str(), (TestOptions->iodirect != 0) );

    //cout << T << endl;

    Fits::Tile_Dim dim = T.get_Dim();
    if ( *BytesPerElement == 0 ) {
      *xDim = dim.xDim;
      *yDim = dim.yDim;
      *BytesPerElement = dim.elem_size;
      cube->tile_size = (dim.xDim * dim.yDim * dim.elem_size);
    }
    else {
      assert( *xDim == dim.xDim && *yDim == dim.yDim && *BytesPerElement == dim.elem_size );
    }
    *zDim = i;

    cube->cube_size += cube->tile_size;

    cube->tiles.push_back(T);
  }

  // Make sure that our cube is padded if necessary to be page aligned
  
  long psize = umt_getpagesize();
  long remainder = cube->cube_size % psize;

  cube->cube_size += remainder ? (psize - remainder) : 0;

  const int prot = PROT_READ|PROT_WRITE;
  int flags = UMAP_PRIVATE;

  region = umap(NULL, cube->cube_size, prot, flags, ps_read, ps_write);
  if ( region == UMAP_FAILED ) {
      ostringstream ss;
      ss << "umap of " << cube->cube_size << " bytes failed for Cube";
      perror(ss.str().c_str());
      return NULL;
  }

  Cubes[region] = cube;
  return region;
}

void PerFits_free_cube(void* region)
{
  auto it = Cubes.find(region);
  assert( "free_cube: failed to find control object" && it != Cubes.end() );
  Cube* cube = it->second;

  if (uunmap(region, cube->cube_size) < 0) {
    ostringstream ss;
    ss << "uunmap of " << cube->cube_size << " bytes failed on region " << region << ": ";
    perror(ss.str().c_str());
    exit(-1);
  }

  delete cube;

  Cubes.erase(region);
}
}
