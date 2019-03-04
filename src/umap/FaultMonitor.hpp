//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_FaultMonitor_HPP
#define _UMAP_FaultMonitor_HPP


namespace Umap {
class FaultMonitor {
  public:
    FaultMonitor(
          char* _region_base_address
        , uint64_t _region_size_in_bytes
        , bool _read_only
        , uint64_t _page_size
        , uint64_t _max_uffd_events
    );

  private:
    char* m_region_base_address;
    uint64_t m_region_size_in_bytes;
    bool m_read_only;
    uint64_t m_page_size;
    uint64_t m_max_uffd_events;
    int m_uffd_fd;

    void start_page_event_dispatcher( void );

    void stop_page_event_dispatcher( void );
};
} // end of namespace Umap

#endif // _UMAP_FaultMonitor_HPP
