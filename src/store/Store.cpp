//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include "umap/umap.h"
#include "umap/Store.h"
#include "StoreFile.h"

Store* Store::make_store(void* _region_, size_t _rsize_, size_t _alignsize_, int _fd_)
{
  return new StoreFile{_region_, _rsize_, _alignsize_, _fd_};
}
