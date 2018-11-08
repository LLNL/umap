/*
 * This file is part of UMAP.  For copyright information see the COPYRIGHT
 * file in the top level directory, or at 
 * https://github.com/LLNL/umap/blob/master/COPYRIGHT.
 * This program is free software; you can redistribute it and/or modify it

 * the Free Software Foundation) version 2.1 dated February 1999.  This program
 * is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the terms and conditions of the GNU Lesser General
 * Public License for more details.  You should have received a copy of the
 * GNU Lesser General Public License along with this program; if not, write to
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
 */
#include "umap/umap.h"
#include "umap/Store.h"
#include "StoreFile.h"

using namespace std;

Store* Store::make_store(void* _region_, size_t _rsize_, size_t _alignsize_, int _fd_)
{
  return new StoreFile{_region_, _rsize_, _alignsize_, _fd_};
}
