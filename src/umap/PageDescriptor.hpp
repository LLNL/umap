//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_PageDescriptor_HPP
#define _UMAP_PageDescriptor_HPP
#include <iostream>

#include "umap/RegionDescriptor.hpp"

namespace Umap {
  struct PageDescriptor {
    enum State { FREE, FILLING, PRESENT, UPDATING, LEAVING };
    void* page;
    RegionDescriptor* region;
    bool is_dirty;
    State state;

    std::string print_state( void ) const;
    void set_state_free( void );
    void set_state_filling( void );
    void set_state_present( void );
    void set_state_updating( void );
    void set_state_leaving( void );
  };

  std::ostream& operator<<(std::ostream& os, const Umap::PageDescriptor* pd);
} // end of namespace Umap

#endif // _UMAP_PageDescriptor_HPP
