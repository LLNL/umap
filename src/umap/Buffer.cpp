//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2021 Lawrence Livermore National Security, LLC and other
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

namespace Umap {
//
// Called after data has been placed into the page
//
void Buffer::mark_page_as_present(PageDescriptor* pd)
{
  lock();

  pd->set_state_present();

  if ( m_waits_for_state_change )
    pthread_cond_broadcast( &m_state_change_cond );

  unlock();
}

//
// Called after page has been flushed to store and page is no longer present
//
void Buffer::mark_page_as_free( PageDescriptor* pd )
{
  lock();

  UMAP_LOG(Debug, "Removing page: " << pd);
  pd->region->erase_page_descriptor(pd);

  m_present_pages.erase(pd->page);

  pd->set_state_free();
  pd->spurious_count = 0;

  //
  // We only put the page descriptor back onto the free list if it isn't
  // deferred.  Note: It will be marked as deferred when the page is part of a
  // Region that has been unmapped.  It will become undeferred later when the
  // eviction manager takes it off the end of the end of the buffer.
  //
  if ( ! pd->deferred )
    release_page_descriptor(pd);

  if ( m_waits_for_state_change )
    pthread_cond_broadcast( &m_state_change_cond );

  pd->page = nullptr;

  unlock();
}

void Buffer::release_page_descriptor( PageDescriptor* pd )
{
    m_free_pages.push_back(pd);

    if ( m_waits_for_avail_pd )
      pthread_cond_broadcast(&m_avail_pd_cond);
}

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
      release_page_descriptor(pd);
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
std::vector<PageDescriptor*> Buffer::evict_oldest_pages()
{
  std::vector<PageDescriptor*> evicted_pages;
  std::vector<PageDescriptor*> pending_pages;
  const int max_num_evicted_pages = 1;
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
    m_stats.events_processed ++;
    unlock();
  }
  else {
    m_rm.get_evict_manager()->EvictAll();
  }
}

bool Buffer::low_threshold_reached( void )
{
  return m_busy_pages.size() <= m_evict_low_water;
}

PageDescriptor::State Buffer::wait_existence_page_state(PageDescriptor* pd){
  PageDescriptor::State ret = pd->state;
  while( ret!=PageDescriptor::State::PRESENT && ret!=PageDescriptor::State::FREE ){
    ++m_stats.waits;
    ++m_waits_for_state_change;

    ++m_stats.waits;
    ++m_waits_for_state_change;
    pthread_cond_wait(&m_state_change_cond, &m_mutex);

    --m_waits_for_state_change;
    ret = pd->state;
  }
  return ret;
}

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

  
void *Buffer::process_page_event(char* paddr, bool iswrite, RegionDescriptor* rd, void *c_uffd)
{
  WorkItem work;
  PageDescriptor *pd = nullptr;
  work.type = Umap::WorkItem::WorkType::NONE;

  lock();
  while(pd==nullptr){
  pd = page_already_present(paddr);

  if ( pd != nullptr ) {  // Page is already present
    if (iswrite && pd->dirty == false) {
      work.page_desc = pd;
      work.c_uffd = c_uffd;
      pd->dirty = true;
      pd->set_state_updating();
      UMAP_LOG(Debug, "PRE: " << pd << " From: " << this);
    }
    else {
      static int hiwat = 0;
      pd->spurious_count++;
      if (pd->spurious_count > hiwat) {
        hiwat = pd->spurious_count;
        UMAP_LOG(Debug, "New Spurious cound high water mark: " << hiwat);
      }
      UMAP_LOG(Debug, "SPU: " << pd << " From: " << this);
      unlock();
      return pd->page;
    }
  }
  else {                  // This page has not been brought in yet
    pd = get_page_descriptor(paddr, rd);
    if(pd==nullptr){
      continue;
    }
    pd->data_present = false;
    work.page_desc = pd;
    work.c_uffd = c_uffd;

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
  }
  unlock();
  return NULL;
}

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
    if(page_already_present(vaddr)){
      return nullptr;
    }
  }

  PageDescriptor* rval;

  rval = m_free_pages.back();
  m_free_pages.pop_back();

  rval->page = vaddr;
  rval->region = rd;
  rval->dirty = false;
  rval->deferred = false;
  rval->set_state_filling();
  rval->spurious_count = 0;

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

/*    UMAP_LOG(Info, "m_size = " << m_size
	     << ", num_busy_pages = " << m_busy_pages.size()
	     << ", num_free_pages = " << m_free_pages.size()
	     << ", events_processed = " << m_stats.events_processed );
*/
    sleep(monitor_interval);

  }//End of loop
}

void Buffer::adapt_free_pages(void)
{
  const int monitor_interval = m_rm.get_adaptive_buffer_freq();
  UMAP_LOG(Info, "every "<< monitor_interval<<" seconds");

  const uint64_t mem_margin_kb = 4194304;//4 GB
  const uint64_t psize = m_rm.get_umap_page_size();
  const size_t num_pages_margin = (size_t)67108864/psize;//64 MB

  // track the used memory in the last epoch
  size_t avg_filled_pages_per_epoch = 0;
  size_t num_busy_pages_old = 0;

  /* start the monitoring loop */
  while( is_adaptor_on ){

    /* Check the current max number of pages allowed in memory */
    /* We use AvailableMemory here because it will evict cached data */

    uint64_t mem_free_kb = 0;
    std::string token;
    std::ifstream file("/proc/meminfo");
    while (file >> token) {
      unsigned long mem;
      if (token == "MemAvailable:") {
        if (file >> mem) {
          mem_free_kb = mem;
        } else {
          UMAP_ERROR("UMAP unable to determine MemAvailable\n");
        }
      }
      // ignore rest of the line
      file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    mem_free_kb = (mem_free_kb > mem_margin_kb) ?(mem_free_kb-mem_margin_kb) : 0;
    uint64_t max_num_free_pages = mem_free_kb * 1024 / psize;

    size_t num_busy_pages = m_busy_pages.size();
    size_t num_free_pages = m_free_pages.size();
    size_t num_pending_pages = m_size - num_busy_pages;//include filling and free pages
    avg_filled_pages_per_epoch = (num_busy_pages>num_busy_pages_old) ?(num_busy_pages - num_busy_pages_old) : 0;
    num_busy_pages_old = num_busy_pages;

/*    UMAP_LOG(Info, "m_size = " << m_size
	     << ", num_busy_pages = " << num_busy_pages
	     << ", num_free_pages = " << num_free_pages
	     << ", num_pending_pages = " << num_pending_pages
	     << ", max_num_free_pages = " << max_num_free_pages
	     << ", avg_filled_pages_per_epoch = " << avg_filled_pages_per_epoch
	     << ", events_processed= " << m_stats.events_processed );
*/
    if( num_pending_pages < max_num_free_pages){

      //Enlarge the buffer if it is nearly exhausted and there is free memory
      if( (num_pending_pages + avg_filled_pages_per_epoch*3 + num_pages_margin) <= max_num_free_pages)
      {
        uint64_t diff = max_num_free_pages - (m_size - m_busy_pages.size());

        /*TODO: free in destructor */
        PageDescriptor *m_array_new = (PageDescriptor *)calloc(diff, sizeof(PageDescriptor));
        if ( m_array_new == nullptr ){
          //do nothing 
          UMAP_LOG(Info, "Failed to allocate additional " << diff << " page descriptors");
        }else{

          lock();
          for ( int i = 0; i < diff; ++i )
            m_free_pages.push_back(&m_array_new[i]);

          m_size += diff;
          m_evict_low_water = apply_int_percentage(m_rm.get_evict_low_water_threshold(), m_size);
          m_evict_high_water = apply_int_percentage(m_rm.get_evict_high_water_threshold(), m_size);
          unlock();
/*
          UMAP_LOG(Info, "Increase to free_pages=" << m_free_pages.size()
                      << ", m_busy_pages=" << m_busy_pages.size() 
                      << ", m_size=" << m_size );
*/
        }

      }//End of enlarging the buffer

    }else{
      /* reduce free pages if the buffer is nearly exhausted 
         and free memory is reduced. Start adjusting 3 epoch ahead */
      if ( (max_num_free_pages < (avg_filled_pages_per_epoch*3)) &&
           (num_free_pages > (max_num_free_pages + num_pages_margin)) )
      {
        lock();
        uint64_t diff = (m_size - m_busy_pages.size()) - max_num_free_pages;
        if( diff >= m_free_pages.size() ){
          if(m_busy_pages.size()==0)
            UMAP_ERROR("no free memory for page caching" );

          diff = m_free_pages.size();
        }

        /* TODO: free allocated free pages */
        m_free_pages.resize(m_free_pages.size() - diff);
        m_size = m_size - diff;
        m_evict_low_water = apply_int_percentage(m_rm.get_evict_low_water_threshold(), m_size);
        m_evict_high_water = apply_int_percentage(m_rm.get_evict_high_water_threshold(), m_size);

        // Kick the eviction daemon if the high water mark has been reached
        // Need to kick this before releasing the lock
        if ( m_busy_pages.size() >= m_evict_high_water ) {
          UMAP_LOG(Info, "Kickoff Eviction: m_evict_high_water "<< m_evict_high_water<<" m_evict_low_water "<<m_evict_low_water);
          WorkItem w;
          w.type = Umap::WorkItem::WorkType::THRESHOLD;
          w.page_desc = nullptr;
          m_rm.get_evict_manager()->send_work(w);
        }

        UMAP_LOG(Info, "Reduce to m_size = " << m_size 
          << ", num_free_pages = " << m_free_pages.size()
          << ", num_busy_pages = " << m_busy_pages.size()
          << ", high_water_threshold = " << m_rm.get_evict_high_water_threshold() <<"%");

        unlock();
      }
    }

    sleep(monitor_interval);

  }//End of loop

  //UMAP_LOG(Info, "adapt_free_pages ends");
}


Buffer::Buffer( void )
  :     m_rm(RegionManager::getInstance())
      , m_size(m_rm.get_max_pages_in_buffer())
      , m_waits_for_avail_pd(0)
      , m_waits_for_state_change(0)
{
  m_array = (PageDescriptor *)calloc(m_size, sizeof(PageDescriptor));
  std::cout<<"Size of buffer"<<m_size<<std::endl;
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

  /* start the buffer adapt thread if the user has not specified the buffer size */
  if( m_rm.get_adaptive_buffer_freq()>0 ){
    is_adaptor_on = true;
    int ret = pthread_create( &adaptThread, NULL, AdaptThreadEntryFunc, this);
    if (ret) {
      UMAP_ERROR("Failed to launch buffer adapt thread");
    }
  }else{
    is_adaptor_on = false;
  }
  
}

void Buffer::print_stats( void ) {
  std::cout << m_stats << std::endl;
}

Buffer::~Buffer( void ) {
#ifdef UMAP_DISPLAY_STATS
  std::cout << m_stats << std::endl;
#endif

  if( is_monitor_on ){
    is_monitor_on = false;
    pthread_join( monitorThread , NULL );
  }

  if( is_adaptor_on ){
    is_adaptor_on = false;
    pthread_join( adaptThread , NULL );
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
