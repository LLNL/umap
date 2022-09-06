//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include <iostream>

#include "umap/PageDescriptor.hpp"
#include "umap/RegionDescriptor.hpp"


namespace Umap {
  std::string PageDescriptor::print_state( void ) const {
    switch (state) {
      default:                                    return "???";
      case Umap::PageDescriptor::State::FREE:     return "FREE";
      case Umap::PageDescriptor::State::FILLING:  return "FILLING";
      case Umap::PageDescriptor::State::PRESENT:  return "PRESENT";
      case Umap::PageDescriptor::State::UPDATING: return "UPDATING";
      case Umap::PageDescriptor::State::LEAVING:  return "LEAVING";
    }
  }

  std::ostream& operator<<(std::ostream& os, const Umap::PageDescriptor* pd)
  {
    if (pd != nullptr) {
      os << "{ "
         << (void*)(pd->page)
         << ", "    << pd->print_state();

      if ( pd->dirty )
         os << ", DIRTY";
      //if ( pd->deferred )
      //   os << ", DEFERRED";
      //if ( pd->spurious_count )
         //os << ", spurious: " << pd->spurious_count;

      os << " }";
    }
    else {
      os << "{ nullptr }";
    }
    return os;
  }

  std::ostream& operator<<(std::ostream& os, const Umap::PageDescriptor::State st)
  {
    switch (st) {
      default:                                    os << "???";    break;
      case Umap::PageDescriptor::State::FREE:     os << "FREE";   break;
      case Umap::PageDescriptor::State::FILLING:  os << "FILLING";    break;
      case Umap::PageDescriptor::State::PRESENT:  os << "PRESENT";    break;
      case Umap::PageDescriptor::State::UPDATING: os << "UPDATING";   break;
      case Umap::PageDescriptor::State::LEAVING:  os << "LEAVING";    break;
    }
    return os;
  }
} // end of namespace Umap
