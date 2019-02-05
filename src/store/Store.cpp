//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory
//
// Created by Marty McFadden, 'mcfadden8 at llnl dot gov'
// LLNL-CODE-733797
//
// All rights reserved.
//
// This file is part of UMAP.
//
// For details, see https://github.com/LLNL/umap
// Please also see the COPYRIGHT and LICENSE files for LGPL license.
//////////////////////////////////////////////////////////////////////////////
#include "umap/umap.h"
#include "umap/Store.h"
#include "StoreFile.h"

Store* Store::make_store(void* _region_, size_t _rsize_, size_t _alignsize_, int _fd_)
{
  return new StoreFile{_region_, _rsize_, _alignsize_, _fd_};
}
