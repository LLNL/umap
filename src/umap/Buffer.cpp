//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////

#include <pthread.h>
#include <fstream>        // for reading meminfo

#include "umap/Buffer.hpp"
#include "umap/config.h"
#include "umap/FillWorkers.hpp"
#include "umap/PageDescriptor.hpp"
#include "umap/RegionManager.hpp"
#include "umap/WorkerPool.hpp"
#include "umap/util/Macros.hpp"

#ifdef PROF
#include <chrono>
#endif

namespace Umap {
//
// Called after data has been placed into the page
//
#ifndef LOCK_OPT
void Buffer::mark_page_as_present(PageDescriptor* pd)
{
  lock();

  pd->set_state_present();

  if ( m_waits_for_state_change )
    pthread_cond_broadcast( &m_state_change_cond );

  unlock();
}
#else
void Buffer::mark_page_as_present(PageDescriptor* pd)
{
  //lock();

  pd->set_state_present();

  if ( m_waits_for_state_change )
    pthread_cond_broadcast( &m_state_change_cond );

  //unlock();
}
#endif

//
// Called after page has been flushed to store and page is no longer present
//
#ifndef LOCK_OPT
void Buffer::mark_page_as_free( PageDescriptor* pd )
{
  lock();

  UMAP_LOG(Debug, "Removing page: " << pd);
  pd->region->erase_page_descriptor(pd);

  m_present_pages.erase(pd->page);

  pd->set_state_free();
  //pd->spurious_count = 0;

  //
  // We only put the page descriptor back onto the free list if it isn't
  // deferred.  Note: It will be marked as deferred when the page is part of a
  // Region that has been unmapped.  It will become undeferred later when the
  // eviction manager takes it off the end of the end of the buffer.
  //
  if ( ! pd->deferred ){
    release_page_descriptor(pd);
  }

  if ( m_waits_for_state_change )
    pthread_cond_broadcast( &m_state_change_cond );

  pd->page = nullptr;

  unlock();
}
#else
void Buffer::mark_page_as_free( PageDescriptor* pd )
{
  int err;
  if ( (err=pthread_mutex_lock(&m_free_pages_secondary_mutex)) != 0 )
    UMAP_ERROR("pthread_mutex_lock m_free_pages_secondary failed: " << strerror(err));

  pd->region->erase_page_descriptor(pd);
  m_present_pages.erase(pd->page);
  pd->set_state_free();

  if ( ! pd->deferred )
    m_free_pages_secondary.push_back(pd);
  
  pd->page = nullptr;

  pthread_mutex_unlock(&m_free_pages_secondary_mutex);
  //printf("103 m_free_pages_secondary.size()=%zu \n", m_free_pages_secondary.size());
}
#endif

#ifndef LOCK_OPT 
void Buffer::release_page_descriptor( PageDescriptor* pd )
{
    m_free_pages.push_back(pd);

    if ( m_waits_for_avail_pd )
      pthread_cond_broadcast(&m_avail_pd_cond);
}
#endif

//
// Called from Evict Manager to begin eviction process on oldest present
// page
//
PageDescriptor* Buffer::evict_oldest_page()
{
  PageDescriptor* pd = nullptr;

  lock();

  while ( m_busy_pages.size() != 0 ) {
    pd = m_busy_pages.back();

    // Deferred means that this page was previously evicted as part of an
    // uunmap of a Region.  This means that this page descriptor points to a
    // page that has already been given back to the system so all we need to
    // do is take it off of the busy list and release the descriptor.
    //
    if ( pd->deferred ) {
      UMAP_LOG(Debug, "Deferred Page: " << pd);

      //
      // Make sure that the page has truly been flushed.
      //
      wait_for_page_state(pd, PageDescriptor::State::FREE);

      m_busy_pages.pop_back();
      m_stats.pages_deleted++;

      //
      // Jump to the next page descriptor
      //
#ifndef LOCK_OPT
      release_page_descriptor(pd);
#else
      m_free_pages.push_back(pd);
      if ( m_waits_for_avail_pd )
        pthread_cond_broadcast(&m_avail_pd_cond);
#endif      
      pd = nullptr;
    }
    else {
      UMAP_LOG(Debug, "Normal Page: " << pd);
      wait_for_page_state(pd, PageDescriptor::State::PRESENT);
      m_busy_pages.pop_back();
      m_stats.pages_deleted++;
      pd->set_state_leaving();
      break;
    }
  }

  unlock();
  return pd;
}

//
// Called from Evict Manager to begin eviction process on at most N (=32)
// oldest present (non-deferred) pages without waiting for status change
//
#ifndef LOCK_OPT
std::vector<PageDescriptor*> Buffer::evict_oldest_pages()
{
  std::vector<PageDescriptor*> evicted_pages;
  std::vector<PageDescriptor*> pending_pages;
  const int max_num_evicted_pages = 32;
  int num_evicted_pages = 0;

  lock();
  size_t num_busy_pages = m_busy_pages.size();
  if( num_busy_pages>0 ){
    size_t i = num_busy_pages - 1;
    for(; (i>=0 && num_evicted_pages<max_num_evicted_pages); i--){
        PageDescriptor* pd = m_busy_pages[i];
        if( !pd->deferred && pd->state == PageDescriptor::State::PRESENT ){
          m_stats.pages_deleted++;
          num_evicted_pages ++;

          pd->state = PageDescriptor::State::LEAVING;
          evicted_pages.push_back(pd);
        }else{
          pending_pages.push_back(pd);
        }
        m_busy_pages.pop_back();
    }

    size_t num_pending_pages = pending_pages.size();
    for(size_t k=0; k<num_pending_pages; k++){
      m_busy_pages.push_back(pending_pages[k]);
    }
  }
  unlock();

  return evicted_pages;
}
#else
//
// Called from Evict Manager to begin eviction process on at most N (=32)
// oldest present (non-deferred) pages without waiting for status change
//
std::vector<PageDescriptor*> Buffer::evict_oldest_pages()
{
  std::vector<PageDescriptor*> evicted_pages;
  std::vector<PageDescriptor*> pending_pages;
  size_t num_busy_pages = m_busy_pages.size();
  const int max_num_evicted_pages = (num_busy_pages<32) ?num_busy_pages:32;
  int num_evicted_pages = 0;

  lock();
  num_busy_pages = m_busy_pages.size();
  if( num_busy_pages>0 ){
    int i = num_busy_pages - 1;
    for(; (i>=0 && num_evicted_pages<max_num_evicted_pages); i--){
        PageDescriptor* pd = m_busy_pages[i];
        if( !pd->deferred && pd->state == PageDescriptor::State::PRESENT ){
          m_stats.pages_deleted++;
          num_evicted_pages ++;

          pd->state = PageDescriptor::State::LEAVING;
          evicted_pages.push_back(pd);
        }else{
          pending_pages.push_back(pd);
        }
        m_busy_pages.pop_back();
    }

    size_t num_pending_pages = pending_pages.size();
    for(size_t k=0; k<num_pending_pages; k++){
      m_busy_pages.push_back(pending_pages[k]);
    }
  }
  unlock();

  return evicted_pages;
}
#endif
void Buffer::flush_dirty_pages()
{
  lock();

  for (auto it = m_busy_pages.begin(); it != m_busy_pages.end(); it++) {
    
    if ( (*it)->dirty ) {
      PageDescriptor* pd = *it;
      UMAP_LOG(Debug, "schedule Dirty Page: " << pd);
      wait_for_page_state(pd, PageDescriptor::State::PRESENT);
      m_rm.get_evict_manager()->schedule_flush(pd);
    }
  }

  m_rm.get_evict_manager()->WaitAll();
  
  unlock();
}
  
//
// Called from uunmap by the unmapping thread of the application
//
// The idea is to go through the entire buffer and remove (evict) all pages
// of the given region descriptor.
//
void Buffer::evict_region(RegionDescriptor* rd)
{
  if (m_rm.get_num_active_regions() > 1) {
    lock();
    while ( rd->count() ) {
      auto pd = rd->get_next_page_descriptor();
      if(pd->state != PageDescriptor::State::LEAVING ){
        pd->deferred = true;
        wait_for_page_state(pd, PageDescriptor::State::PRESENT);
        pd->set_state_leaving();
        m_rm.get_evict_manager()->schedule_eviction(pd);
      }
      wait_for_page_state(pd, PageDescriptor::State::FREE);
    }
    unlock();
  }
  else {
    m_rm.get_evict_manager()->EvictAll();
  }
}

#ifndef LOCK_OPT
bool Buffer::low_threshold_reached( void )
{
  return m_busy_pages.size() <= m_evict_low_water;
}
#endif

typedef struct FetchFuncParams {
  uint64_t psize;
  RegionDescriptor* rd;
  Uffd* m_uffd;
  uint64_t offset_st;
  uint64_t offset_end;
} FetchFuncParams;

void *FetchFunc(void *arg) 
{ 
  FetchFuncParams* params = (FetchFuncParams*) arg;
  uint64_t psize = params->psize;
  RegionDescriptor* rd = params->rd;
  Uffd* m_uffd = params->m_uffd;
  char* region_st = rd->start();
  uint64_t offset_st = params->offset_st;
  uint64_t offset_end= params->offset_end;

  char* copyin_buf = (char*) malloc(psize);
  if ( !copyin_buf )
    UMAP_ERROR("Failed to allocate copyin_buf");
  
  for(uint64_t offset = offset_st; offset < offset_end; offset+=psize){
    
    if( rd->store()->read_from_store(copyin_buf, psize, offset) == -1)
      UMAP_ERROR("failed to read_from_store at offset="<<offset);
  
    m_uffd->copy_in_page(copyin_buf, region_st + offset );
  }

  free(copyin_buf);
  return NULL;
} 

void Buffer::fetch_and_pin(char* paddr, uint64_t size)
{
  lock();
  auto rd = m_rm.containing_region(paddr);
  
  if ( rd == nullptr )
    UMAP_ERROR("the prefetched region is not found");

  /* cap the prefetched region */
  char* pend = paddr + size;  
  if( pend > rd->end() ){
    pend = (char*) rd->end();
    UMAP_LOG(Info, "the prefetched rergion is larger than the region (end at "<<pend<<")");
  }

  /* get aligned fetch size */
  uint64_t offset_st = rd->store_offset( paddr );
  uint64_t offset_end = rd->store_offset( pend );
  size = pend - paddr;
  
  /* Check free memory */
  uint64_t mem_avail_kb = 0;
  unsigned long mem;
  std::string token;
  std::ifstream file("/proc/meminfo");
  while (file >> token) {
    if (token == "MemAvailable:") {
      if (file >> mem) {
	mem_avail_kb = mem;
      } else {
        UMAP_ERROR("UMAP unable to determine system memory size\n");
      }
    }
    // ignore rest of the line
    file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  }

  const uint64_t mem_margin_kb = 16777216;
  mem_avail_kb = (mem_avail_kb > mem_margin_kb) ?(mem_avail_kb-mem_margin_kb) : 0;
  

  uint64_t psize = m_rm.get_umap_page_size();
  size_t num_free_pages = m_free_pages.size();
  uint64_t free_page_mem = psize * num_free_pages;
  uint64_t mem_avail = (mem_avail_kb*1024/psize) * psize;

  UMAP_LOG(Info, " MemAvailable = " << mem
	   << " Mem Usable = " << mem_avail
	   << " fetch_and_pin = " << size
	   << " free_page_mem = " << free_page_mem << " (" << num_free_pages <<" x " << psize <<" )");

  /* Reduce the number of free pages if avail mem is insufficient */
  if( ( free_page_mem + size) >= mem_avail ){

    uint64_t reduced_mem = ( free_page_mem + size) - mem_avail;
    if( reduced_mem < free_page_mem){
      size_t new_num_free_pages = (free_page_mem - reduced_mem)/psize;
      m_free_pages.resize(new_num_free_pages);
      
      m_size = m_busy_pages.size() + m_free_pages.size();
      m_evict_low_water = apply_int_percentage(m_rm.get_evict_low_water_threshold(), m_size);
      m_evict_high_water = apply_int_percentage(m_rm.get_evict_high_water_threshold(), m_size);
          
      UMAP_LOG(Info, "Reduced Buffer Size to " << m_size );

    }else{
      /* TODO: evict current pages? */
      UMAP_ERROR("Currently, no support for pinning a region larger than free pages\n");
    }
  }


  /* get page alighed offset*/
  Uffd* m_uffd = m_rm.get_uffd_h();
  size_t num_pages = (offset_end - offset_st)/psize;
  size_t num_fetch_threads = (num_pages>1024) ?8 : 1;
  size_t stride = num_pages/num_fetch_threads*psize;

  time_t start = time(NULL);
  pthread_t fetchThreads[num_fetch_threads];
  FetchFuncParams params[num_fetch_threads];
  for(int i=0; i<num_fetch_threads; i++){
    params[i].psize = psize;
    params[i].rd = rd;
    params[i].m_uffd = m_uffd;
    params[i].offset_st = offset_st + stride*i;
    params[i].offset_end = params[i].offset_st + stride;
    if(i==(num_fetch_threads-1))
      params[i].offset_end = offset_end;
    UMAP_LOG(Info, "FetchThread "<<i<<" ["<<params[i].offset_st<<" , "<<params[i].offset_end<<"]");
      
    int ret = pthread_create(&fetchThreads[i], NULL, FetchFunc, &params[i]);
    if (ret) {
      UMAP_ERROR("Failed to launch fetchthread "<<i );
    }
  }

  for(int i=0; i<num_fetch_threads; i++)
    pthread_join(fetchThreads[i], NULL);

  time_t end = time(NULL);
  UMAP_LOG(Info,"Fetch_and_pin: "<< (end-start) << " seconds");

  unlock();
}


void Buffer::process_page_event(char* paddr, bool iswrite, RegionDescriptor* rd)
{
  WorkItem work;
  work.type = Umap::WorkItem::WorkType::NONE;

  lock();
  auto pd = page_already_present(paddr);

  if ( pd != nullptr ) {  // Page is already present
    if (iswrite && pd->dirty == false) {
      work.page_desc = pd;
      pd->dirty = true;
      pd->set_state_updating();
      UMAP_LOG(Debug, "PRE: " << pd << " From: " << this);
    }
    else {
      static int hiwat = 0;

      /*pd->spurious_count++;
      if (pd->spurious_count > hiwat) {
        hiwat = pd->spurious_count;
        UMAP_LOG(Debug, "New Spurious cound high water mark: " << hiwat);
      }*/

      UMAP_LOG(Debug, "SPU: " << pd << " From: " << this);
      unlock();
      return;
    }
  }
  else {                  // This page has not been brought in yet
    pd = get_page_descriptor(paddr, rd);
    pd->data_present = false;
    work.page_desc = pd;

    rd->insert_page_descriptor(pd);
    m_present_pages[pd->page] = pd;

    if (iswrite)
      pd->dirty = true;

    UMAP_LOG(Debug, "NEW: " << pd << " From: " << this);
  }

  m_rm.get_fill_workers_h()->send_work(work);

  //
  // Kick the eviction daemon if the high water mark has been reached
  //
  if ( m_busy_pages.size() == m_evict_high_water ) {
    WorkItem w;

    w.type = Umap::WorkItem::WorkType::THRESHOLD;
    w.page_desc = nullptr;
    m_rm.get_evict_manager()->send_work(w);
  }

  m_stats.events_processed ++;
  unlock();
}
#ifdef LOCK_OPT
void Buffer::fast_drain(){
  while(m_free_pages.size()==0){
    usleep(10);
  //UMAP_LOG(Info, "m_free_pages_secondary.size()= "<<m_free_pages_secondary.size()<<")");
  // first check the secondary free buffer by Evictors
  if( m_free_pages_secondary.size() > 8){
    int err;   
    if ( (err = pthread_mutex_lock(&m_free_pages_secondary_mutex)) != 0 )
      UMAP_ERROR("pthread_mutex_lock m_free_pages_secondary failed: " << strerror(err));

    m_free_pages = m_free_pages_secondary;
    m_free_pages_secondary.clear();

    pthread_mutex_unlock(&m_free_pages_secondary_mutex);
    //printf("520 m_free_pages_secondary.size()=%zu m_free_pages.size()=%zu\n", m_free_pages_secondary.size(), m_free_pages.size());
  }
  // otherwise quickly drop N clean pages from busy_pages
  else if(0){
    std::vector<PageDescriptor*> pending_pages;
    
    //if ( (err = pthread_mutex_lock(&m_busy_pages_mutex)) != 0 )
      //UMAP_ERROR("pthread_mutex_lock m_busy_pages failed: " << strerror(err));

    size_t num_busy_pages = m_busy_pages.size();
    assert( num_busy_pages>0 );
    UMAP_LOG(Info, "num_busy_pages = "<<num_busy_pages );

    int max_num_freed_pages = (num_busy_pages>32) ?32 :num_busy_pages;
    int num_evicted_pages = 0;
    size_t page_size = m_rm.get_umap_page_size();
    for(int i = num_busy_pages - 1; (i>=0 && num_evicted_pages<max_num_freed_pages); i--){
        PageDescriptor* pd = m_busy_pages[i];UMAP_LOG(Info, "i = "<<i << pd );
        if( !pd->deferred && pd->state == PageDescriptor::State::PRESENT && !pd->dirty ){
          if (madvise(pd->page, page_size, MADV_DONTNEED) == -1)
            UMAP_ERROR("madvise failed: " << errno << " (" << strerror(errno) << ")");
          pd->region->erase_page_descriptor(pd);
          pd->state = PageDescriptor::State::FREE;
          pd->page  = nullptr;
          m_present_pages.erase(pd->page);
          m_free_pages.push_back(pd);
        }else{
          pending_pages.push_back(pd);
        }
        m_busy_pages.pop_back();
    }
    for(auto it : pending_pages){
      m_busy_pages.push_back(it);
    }
    //pthread_mutex_lock(&m_busy_pages_mutex);
  }
  }
}

void Buffer::process_page_events(RegionDescriptor* rd, char** paddrs, bool *iswrites, int num_pages)
{
  #ifdef PROF1
  int num_pages_old = num_pages;
  auto t0 = std::chrono::steady_clock::now();
  #endif

  while ( num_pages>0 ) {
    int pivot = 0;
    for( int p=0; p<num_pages; p++){
      char* paddr = paddrs[p];
      bool iswrite= iswrites[p];
      //UMAP_LOG(Info, "paddr = " <<(void*)paddr << " iswrite="<<iswrite);

      //auto pd = page_already_present(paddr);
      //auto pp = m_present_pages.find(paddr);
      #ifdef PROF1
      auto t2 = std::chrono::steady_clock::now();
      #endif

      PageDescriptor* pd = rd->find(paddr);    

      //
      // Most likely case
      //
      if ( pd == nullptr ){  // This page has not been brought in yet
        
        //pd = get_page_descriptor(paddr, rd); // may stall, get from free_pages
        {
          if ( m_free_pages.size() == 0 )  {
            fast_drain();
          }

          pd = m_free_pages.back();
          m_free_pages.pop_back();
          pd->page = paddr;
          pd->region = rd;
          pd->state = PageDescriptor::State::FILLING;         
          pd->dirty = iswrite;
          pd->deferred = false;
          pd->data_present = false;
          m_busy_pages.push_front(pd);
        }
        #ifdef PROF1
        auto t22 = std::chrono::steady_clock::now();
        #endif
        rd->insert_page_descriptor(pd);
        #ifdef PROF1
        auto t23 = std::chrono::steady_clock::now();
        UMAP_LOG(Info, "insert_page_descriptor \t" << (std::chrono::duration_cast<std::chrono::nanoseconds>(t23-t22).count()) );
        #endif
        m_present_pages[pd->page] = pd;
        
        WorkItem work;
        work.type = Umap::WorkItem::WorkType::NONE;
        work.page_desc = pd;
        #ifdef PROF1
        auto t24 = std::chrono::steady_clock::now();
        #endif
        #ifdef PROF
        work.timing = std::chrono::steady_clock::now();
        #endif
        m_rm.get_fill_workers_h()->send_work(work);
        #ifdef PROF1
        auto t25 = std::chrono::steady_clock::now();
        UMAP_LOG(Info, "send_work \t" << (std::chrono::duration_cast<std::chrono::nanoseconds>(t25-t24).count()) );
        #endif
        //UMAP_LOG(Info, "NEW: " << pd << " From: " << this);        
      }
      //
      // Next most likely is that it is just present in the buffer
      //
      else if ( pd->state == PageDescriptor::State::PRESENT ){
        // Page is already present
        if (iswrite && pd->dirty == false) {
          WorkItem work;
          work.type = Umap::WorkItem::WorkType::NONE;
          work.page_desc = pd;
          pd->dirty = true;
          pd->set_state_updating();
          //UMAP_LOG(Debug, "PRE: " << pd << " From: " << this);
          m_rm.get_fill_workers_h()->send_work(work);
        }
      }
      // There is a chance that the state of this page is not/no-longer
      // PRESENT.  If this is the case, we need to wait for it to finish
      // with whatever is happening to it and then check again
      //
      else if ( pd->state != PageDescriptor::State::LEAVING ){
        /*UMAP_LOG(Debug, "Waiting for state: (ANY)" << ", " << pp->second);

        ++m_stats.waits;
        ++m_waits_for_state_change;
        pthread_cond_wait(&m_state_change_cond, &m_mutex);
        --m_waits_for_state_change;*/

        // copy aside to re-visit later
        paddrs[pivot] = paddr;
        iswrites[pivot] = iswrite;
        pivot ++;
      }
      
      #ifdef PROF1
      auto t3 = std::chrono::steady_clock::now();
      UMAP_LOG(Info, "paddr "<< paddr << " \t" << (std::chrono::duration_cast<std::chrono::nanoseconds>(t3-t2).count()) );
      #endif  
    }
    num_pages = pivot;
  }
  #ifdef PROF1
  auto t1 = std::chrono::steady_clock::now();
  UMAP_LOG(Info, "num_pages "<< num_pages_old << " \t" << (std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count()) );
  #endif
  
  // Kick the eviction daemon if the high water mark has been reached
  //
  if ( m_busy_pages.size() >= m_evict_high_water ) {
    WorkItem w;

    w.type = Umap::WorkItem::WorkType::THRESHOLD;
    w.page_desc = nullptr;
    m_rm.get_evict_manager()->send_work(w);
  }

}
#endif

// Return nullptr if page not present, PageDescriptor * otherwise
PageDescriptor* Buffer::page_already_present( char* page_addr )
{
  while (1) {
    auto pp = m_present_pages.find(page_addr);
  
    //
    // Most likely case
    //
    if ( pp == m_present_pages.end() )
      return nullptr;

    //
    // Next most likely is that it is just present in the buffer
    //
    if ( pp->second->state == PageDescriptor::State::PRESENT )
      return pp->second;

    // There is a chance that the state of this page is not/no-longer
    // PRESENT.  If this is the case, we need to wait for it to finish
    // with whatever is happening to it and then check again
    //
    UMAP_LOG(Debug, "Waiting for state: (ANY)" << ", " << pp->second);

    ++m_stats.waits;
    ++m_waits_for_state_change;
    pthread_cond_wait(&m_state_change_cond, &m_mutex);
    --m_waits_for_state_change;
  }
}

PageDescriptor* Buffer::get_page_descriptor(char* vaddr, RegionDescriptor* rd)
{
  while ( m_free_pages.size() == 0 )  {
    ++m_waits_for_avail_pd;
    m_stats.not_avail++;

    ++m_stats.waits;
    ++m_waits_for_state_change;
    pthread_cond_wait(&m_avail_pd_cond, &m_mutex);

    --m_waits_for_avail_pd;
  }

  PageDescriptor* rval;

  rval = m_free_pages.back();
  m_free_pages.pop_back();

  rval->page = vaddr;
  rval->region = rd;
  rval->dirty = false;
  rval->deferred = false;
  rval->set_state_filling();
  //rval->spurious_count = 0;

  m_stats.pages_inserted++;
  m_busy_pages.push_front(rval);

  return rval;
}

uint64_t Buffer::apply_int_percentage( int percentage, uint64_t item )
{
  uint64_t rval;

  if ( percentage < 0 || percentage > 100)
    UMAP_ERROR("Invalid percentage (" << percentage << ") given");

  if ( percentage == 0 || percentage == 100 ) {
    rval = item;
  }
  else {
    float f = (float)((float)percentage / (float)100.0);
    rval = f * item;
  }
  return rval;
}
#ifndef LOCK_OPT
void Buffer::lock()
{
  int err;
  if ( (err = pthread_mutex_trylock(&m_mutex)) != 0 ) {
    if (err != EBUSY)
      UMAP_ERROR("pthread_mutex_trylock failed: " << strerror(err));

    if ( (err = pthread_mutex_lock(&m_mutex)) != 0 )
      UMAP_ERROR("pthread_mutex_lock failed: " << strerror(err));
    m_stats.lock_collision++;
  }
  m_stats.lock++;
}
#else
void Buffer::lock()
{
  int err;
  if ( (err = pthread_mutex_lock(&m_mutex)) != 0 )
    UMAP_ERROR("pthread_mutex_lock failed: " << strerror(err));
}
#endif
void Buffer::unlock()
{
  pthread_mutex_unlock(&m_mutex);
}

void Buffer::wait_for_page_state( PageDescriptor* pd, PageDescriptor::State st)
{
  UMAP_LOG(Debug, "Waiting for state: " << st << ", " << pd);

  while ( pd->state != st ) {
    ++m_stats.waits;
    ++m_waits_for_state_change;

    pthread_cond_wait(&m_state_change_cond, &m_mutex);

    --m_waits_for_state_change;
  }
}

void Buffer::monitor(void)
{
  const int monitor_interval = m_rm.get_monitor_freq();
  UMAP_LOG(Info, "every " << monitor_interval << " seconds");

  /* start the monitoring loop */
  while( is_monitor_on ){

    UMAP_LOG(Info, "m_size = " << m_size
	     << ", num_busy_pages = " << m_busy_pages.size()
	     << ", num_free_pages = " << m_free_pages.size()
	     << ", events_processed = " << m_stats.events_processed );

    sleep(monitor_interval);

  }//End of loop

}

Buffer::Buffer( void )
  :     m_rm(RegionManager::getInstance())
      , m_size(m_rm.get_max_pages_in_buffer())
      , m_waits_for_avail_pd(0)
      , m_waits_for_state_change(0)
{
  m_array = (PageDescriptor *)calloc(m_size, sizeof(PageDescriptor));
  if ( m_array == nullptr )
    UMAP_ERROR("Failed to allocate " << m_size*sizeof(PageDescriptor)
        << " bytes for buffer page descriptors");

  for ( int i = 0; i < m_size; ++i )
    m_free_pages.push_back(&m_array[i]);

  pthread_mutex_init(&m_mutex, NULL);
  pthread_cond_init(&m_avail_pd_cond, NULL);
  pthread_cond_init(&m_state_change_cond, NULL);

  m_evict_low_water = apply_int_percentage(m_rm.get_evict_low_water_threshold(), m_size);
  m_evict_high_water = apply_int_percentage(m_rm.get_evict_high_water_threshold(), m_size);

  /* monitor page stats periodically */
  if( m_rm.get_monitor_freq()>0 ){
    is_monitor_on = true;
    int ret = pthread_create( &monitorThread, NULL, MonitorThreadEntryFunc, this);
    if (ret) {
      UMAP_ERROR("Failed to launch the monitor thread");
    }
  }else{
    is_monitor_on = false;
  }

}

Buffer::~Buffer( void ) {
#ifdef UMAP_DISPLAY_STATS
  std::cout << m_stats << std::endl;
#endif

  if( is_monitor_on ){
    is_monitor_on = false;
    pthread_join( monitorThread , NULL );
  }
  
  assert("Pages are still present" && m_present_pages.size() == 0);
  pthread_cond_destroy(&m_avail_pd_cond);
  pthread_cond_destroy(&m_state_change_cond);
  pthread_mutex_destroy(&m_mutex);
  free(m_array);
}

std::ostream& operator<<(std::ostream& os, const Umap::Buffer* b)
{
  if ( b != nullptr ) {
    os << "{ m_size: " << b->m_size
      << ", m_waits_for_avail_pd: " << b->m_waits_for_avail_pd
      << ", m_present_pages.size(): " << std::setw(2) << b->m_present_pages.size()
      << ", m_free_pages.size(): " << std::setw(2) << b->m_free_pages.size()
      << ", m_busy_pages.size(): " << std::setw(2) << b->m_busy_pages.size()
      << " }"
      ;
  }
  else {
    os << "{ nullptr }";
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const Umap::BufferStats& stats)
{
  os << "Buffer Statisics:\n"
    << "   Pages Inserted: " << std::setw(12) << stats.pages_inserted<< "\n"
    << "    Pages Deleted: " << std::setw(12) << stats.pages_deleted<< "\n"
    << " Unavailable wait: " << std::setw(12) << stats.not_avail<< "\n"
    << "            Locks: " << std::setw(12) << stats.lock << "\n"
    << "  Lock collisions: " << std::setw(12) << stats.lock_collision << "\n"
    << "            waits: " << std::setw(12) << stats.waits;
  return os;
}
} // end of namespace Umap
