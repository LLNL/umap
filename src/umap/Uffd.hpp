//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_Uffd_HPP
#define _UMAP_Uffd_HPP

#include <cstdint>

#include <vector>

#include "linux/userfaultfd.h"
#include "umap/config.h"
#include "umap/util/Macros.hpp"

namespace Umap {
class Uffd {
  public:
    Uffd(   char*    region
          , uint64_t region_size
          , uint64_t max_fault_events
          , uint64_t page_size
    );

    ~Uffd( void );

    bool       get_page_events( void );  // False: if timed out waiting
    const bool next_page_event( void );

    char* get_event_page_address( void );
    void  enable_write_protect(char* page_address);
    void disable_write_protect(char* page_address);

    bool is_write_protect_event( void );
    bool   is_write_fault_event( void );
    bool    is_read_fault_event( void );

    void                   copy_in_page(char* data, char* page_address);
    void copy_in_page_and_write_protect(char* data, char* page_address);

  private:
    char*    m_region;
    uint64_t m_region_size;
    uint64_t m_max_fault_events;
    uint64_t m_page_size;
    int      m_uffd_fd;

    std::vector<uffd_msg> m_events;
    uffd_msg* m_cur_event;
    uffd_msg* m_last_event;

    void check_uffd_compatibility( void );
    void register_with_uffd( void );
    void unregister_from_uffd(void);
};
} // end of namespace Umap

#endif // _UMAP_Uffd_HPP
