//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_PageDescriptor_HPP
#define _UMAP_PageDescriptor_HPP

#include <iostream>
#include <string>

#include "umap/util/Macros.hpp"

namespace Umap {
  class RegionDescriptor;

  struct PageDescriptor {
    enum State { FREE = 0, FILLING, PRESENT, UPDATING, LEAVING };
    char*             page;
    RegionDescriptor* region;
    State             state;
    bool              dirty;
    //bool              deferred;
    bool              data_present;
    int               spurious_count;
    pthread_mutex_t   m_mutex;

    std::string print_state( void ) const;

    inline void set_state_free( void ) {
      if ( state != LEAVING )
        UMAP_ERROR("Invalid state transition from: " << print_state() << " " << this);
      state = FREE;
    }

    inline void set_state_filling( void ) {
      if ( state != FREE )
        UMAP_ERROR("Invalid state transition from: " << print_state());
      state = FILLING;
    }

    inline void set_state_present( void ) {
      if ( state != FILLING && state != UPDATING )
        UMAP_ERROR("Invalid state transition from: " << print_state() << " " << this);
      state = PRESENT;
    }

    inline void set_state_updating( void ) {
      if ( state != PRESENT )
        UMAP_ERROR("Invalid state transition from: " << print_state());
      state = UPDATING;
    }

    inline void set_state_leaving( void ) {
      if ( state != PRESENT )
        UMAP_ERROR("Invalid state transition from: " << print_state());
      state = LEAVING;
    }

  };

  std::ostream& operator<<(std::ostream& os, const Umap::PageDescriptor::State st);
  std::ostream& operator<<(std::ostream& os, const Umap::PageDescriptor* pd);
} // end of namespace Umap

#endif // _UMAP_PageDescriptor_HPP
