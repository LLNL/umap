//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_PageDescriptor_HPP
#define _UMAP_PageDescriptor_HPP

#include "umap/config.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <pthread.h>
#include <queue>
#include <time.h>
#include <unordered_map>
#include <vector>

#include "umap/util/Macros.hpp"

namespace Umap {
  struct PageDescriptor {
    enum State { FREE, FILLING, PRESENT, UPDATING, LEAVING };
    void* m_page;
    bool m_is_dirty;
    State m_state;

    bool page_is_dirty() { return m_is_dirty; }
    void mark_page_dirty() { m_is_dirty = true; }
    void* get_page_addr() { return m_page; }

    std::string print_state() const
    {
      switch (m_state) {
        default:                                    return "???";
        case Umap::PageDescriptor::State::FREE:     return "FREE";
        case Umap::PageDescriptor::State::FILLING:  return "FILLING";
        case Umap::PageDescriptor::State::PRESENT:  return "PRESENT";
        case Umap::PageDescriptor::State::UPDATING: return "UPDATING";
        case Umap::PageDescriptor::State::LEAVING:  return "LEAVING";
      }
    }

    void set_state_free() {
      if ( m_state != LEAVING )
        UMAP_ERROR("Invalid state transition from: " << print_state());
      m_state = FREE;
    }

    void set_state_filling() {
      if ( m_state != FREE )
        UMAP_ERROR("Invalid state transition from: " << print_state());
      m_state = FILLING;
    }

    void set_state_present() {
      if ( m_state != FILLING && m_state != UPDATING )
        UMAP_ERROR("Invalid state transition from: " << print_state());
      m_state = PRESENT;
    }

    void set_state_updating() {
      if ( m_state != PRESENT )
        UMAP_ERROR("Invalid state transition from: " << print_state());
      m_state = UPDATING;
    }

    void set_state_leaving() {
      if ( m_state != PRESENT )
        UMAP_ERROR("Invalid state transition from: " << print_state());
      m_state = LEAVING;
    }
  };

  std::ostream& operator<<(std::ostream& os, const Umap::PageDescriptor* pd)
  {
    if (pd != nullptr) {
      os << "{ m_page: " << (void*)(pd->m_page)
         << ", m_state: " << pd->print_state()
         << ", m_is_dirty: " << pd->m_is_dirty << " }";
    }
    else {
      os << "{ nullptr }";
    }
    return os;
  }
} // end of namespace Umap

#endif // _UMAP_PageDescriptor_HPP
