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

namespace Umap {

  int WorkerPool::queue_id_g = 0;
  
//
// Called after data has been placed into the page
//

void Buffer::mark_page_as_present(PageDescriptor* pd)
{

  //lock();

  pd->set_state_present();

  if ( m_waits_for_present_state_change )
    pthread_cond_broadcast( &m_present_state_change_cond );

  //unlock();

}


//
// Called after page has been flushed to store and page is no longer present
//

void Buffer::mark_page_as_free( PageDescriptor* pd )
{
  int page_ratio = pd->region->page_size()/m_psize;
  pd->region->erase_page_descriptor(pd);

  int err;
  if ( (err=pthread_mutex_lock(&m_free_pages_secondary_mutex)) != 0 )
    UMAP_ERROR("pthread_mutex_lock m_free_pages_secondary failed: " << strerror(err));

  m_free_pages_secondary.push_back(pd);
  m_free_page_secondary_size.store(m_free_page_secondary_size.load() + page_ratio);

  //if ( m_waits_for_free_state_change )
    //pthread_cond_broadcast( &m_free_state_change_cond );
    
  pthread_mutex_unlock(&m_free_pages_secondary_mutex);

}

//
// Called from Evict Manager to begin eviction process on at most N (=32)
// oldest present (non-deferred) pages 
// return at least one page unless m_busy_pages is empty
//
std::vector<PageDescriptor*> Buffer::evict_oldest_pages()
{
  std::vector<PageDescriptor*> evicted_pages;

  //need lock before modifying m_busy_pages
  lock();

  while( m_busy_pages.size()>0 ) {
    
    // LRU
    PageDescriptor* pd = m_busy_pages.back();

    if( pd->state == PageDescriptor::State::PRESENT ){
      m_busy_pages.pop_back();
      m_busy_page_size -= pd->region->page_size() / m_psize;
      pd->set_state_leaving();
      evicted_pages.push_back(pd);      
      if( evicted_pages.size()==32 ) 
        break;

    }else if(pd->state == PageDescriptor::State::FILLING || pd->state == PageDescriptor::State::UPDATING){
      //If the lastest page is still updating/filling, pause the eviction process
      //This has huge impact on performance, don't wait for any status update
      /*if( evicted_pages.size()==0 ){
        wait_for_page_present_state(pd);
        pd->set_state_leaving();
        evicted_pages.push_back(pd);
        m_busy_pages.pop_back();
        m_busy_page_size -= pd->region->page_size() / m_psize;
      }*/
      break;
    }else if(pd->state == PageDescriptor::State::LEAVING || pd->state == PageDescriptor::State::FREE) {
      UMAP_ERROR(" Invalid page status in busy list" << this << pd);
      break;
    }    
  }

  unlock();

  /*UMAP_LOG(Info, " Evicted_pages=" << evicted_pages.size() );
  for(auto pd : evicted_pages)
    UMAP_LOG(Info, pd);
  */

  return evicted_pages;
}

void Buffer::flush_dirty_pages()
{
  while ( !low_threshold_reached() ){ 
    if( m_rm.get_evict_manager()->wq_is_empty() ){
      WorkItem w;
      w.type = Umap::WorkItem::WorkType::THRESHOLD;
      w.page_desc = nullptr;
      m_rm.get_evict_manager()->send_work(w);
    }
    sleep(1);
  }//critical, avoid race condition of equeue workitems for evictors

  lock();

  for (auto it = m_busy_pages.begin(); it != m_busy_pages.end(); it++) {
    
    if ( (*it)->dirty ) {
      PageDescriptor* pd = *it;
      wait_for_page_present_state(pd);
      m_rm.get_evict_manager()->schedule_flush(pd);
    }
  }
  
  bool done = false;
  while ( !done ){
    done = true;
    for (auto it = m_busy_pages.begin(); it != m_busy_pages.end(); it++) {
      if ( (*it)->dirty ) {
        done = false;
        sleep(1);
        break;
      }
    }
  }
  
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
  while ( !low_threshold_reached() ){ 
    if( m_rm.get_evict_manager()->wq_is_empty() ){
      WorkItem w;
      w.type = Umap::WorkItem::WorkType::THRESHOLD;
      w.page_desc = nullptr;
      m_rm.get_evict_manager()->send_work(w);
    }
    sleep(1);
  }//critical, avoid race condition of equeue workitems for evictors

  lock();
  std::unordered_map<char*, PageDescriptor*> present_pages = rd->get_present_pages();
  uint64_t pages_per_region_page = (rd->page_size() / m_psize);

  for(std::deque<PageDescriptor*>::iterator it = m_busy_pages.begin(); it != m_busy_pages.end(); ) {
    PageDescriptor* pd = *it;
    if (rd->find(pd->page) != nullptr ) {
        wait_for_page_present_state(pd);
        it = m_busy_pages.erase(it);
        m_busy_page_size -= pages_per_region_page;
        pd->set_state_leaving();
        //UMAP_LOG(Info, pd);
        m_rm.get_evict_manager()->schedule_eviction(pd);
    } else {
        ++it;
    }
  }
    
  unlock();
  
  while(rd->has_present_pages()){
    //present_pages = rd->get_present_pages();
    //for( auto it : present_pages)
      //UMAP_LOG(Info, it.second);
    sleep(1);
  }//faster than wait on condition
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
  
    m_uffd->copy_in_page(copyin_buf, region_st + offset , psize );
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
    UMAP_LOG(Info, "the prefetched region is larger than the region (end at "<<pend<<")");
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

  size_t  num_free_pages = m_free_page_size + m_free_page_secondary_size;
  uint64_t free_page_mem = m_psize * num_free_pages;
  uint64_t     mem_avail = (mem_avail_kb*1024/m_psize) * m_psize;

  UMAP_LOG(Info, " MemAvailable = " << mem
	   << " Mem Usable = " << mem_avail
	   << " fetch_and_pin = " << size
	   << " free_page_mem = " << free_page_mem << " (" << num_free_pages <<" x " << m_psize <<" )");

  /* Reduce the number of free pages if avail mem is insufficient */
  if( ( free_page_mem + size) >= mem_avail ){

    uint64_t reduced_mem = ( free_page_mem + size) - mem_avail;
    if( reduced_mem < free_page_mem){
      size_t new_num_free_pages = (free_page_mem - reduced_mem)/m_psize;
      m_free_pages.resize(new_num_free_pages);
      
      adjust_hi_lo_watermark();          

    }else{
      /* TODO: evict current pages? */
      UMAP_ERROR("Currently, no support for pinning a region larger than free pages\n");
    }
  }


  /* get page aligned offset */
  Uffd* m_uffd = m_rm.get_uffd_h();
  size_t num_pages = (offset_end - offset_st)/m_psize;
  size_t num_fetch_threads = (num_pages>1024) ?8 : 1;
  size_t stride = num_pages/num_fetch_threads*m_psize;

  pthread_t fetchThreads[num_fetch_threads];
  FetchFuncParams params[num_fetch_threads];
  for(int i=0; i<num_fetch_threads; i++){
    params[i].psize = m_psize;
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

  unlock();
}

void Buffer::process_page_events(RegionDescriptor* rd, char** paddrs, bool *iswrites, int num_pages)
{
  lock();
  uint64_t region_page_size  = rd->page_size();
  int      region_page_ratio = region_page_size/m_psize;

  int remaining_pages = num_pages;
  while( remaining_pages>0 ){
    int pivot = 0;
    for( int p = 0; p < remaining_pages; p++ ){
      char*  paddr = paddrs[p];
      bool iswrite = iswrites[p];
      PageDescriptor* pd = rd->find(paddr);

      /*if( pd != nullptr ) {
        UMAP_LOG(Info, "paddr " <<(void*)paddr << " iswrite=" << iswrite << pd);
      } else {
        UMAP_LOG(Info, "paddr " <<(void*)paddr << " iswrite=" << iswrite);
      } */
      //
      // Most likely case
      //
      if ( pd == nullptr ){  // This page has not been brought in yet
        
        //pd = get_page_descriptor(paddr, rd); // may stall, waiting for free_pages
        {
          if ( m_free_page_size < region_page_ratio ){
            if( m_rm.get_evict_manager()->wq_is_empty() ){
              WorkItem w;
              w.type = Umap::WorkItem::WorkType::THRESHOLD;
              w.page_desc = nullptr;
              m_rm.get_evict_manager()->send_work(w);
            }
            unlock(); //release control to EvictMgr to modify busy_list
            
            int t = m_free_page_secondary_size.load();
            //while( (t+m_free_page_size) < region_page_ratio ){t = m_free_page_secondary_size.load();}
            if( (t+m_free_page_size) >= region_page_ratio ){
              pthread_mutex_lock(&m_free_pages_secondary_mutex);
              lock(); //take control back from EvictMgr to modify free_list

              for(auto it : m_free_pages_secondary)
                m_free_pages.push_back(it);
              m_free_page_size += m_free_page_secondary_size.load();
              m_free_pages_secondary.clear();
              m_free_page_secondary_size.store(0);

              pthread_mutex_unlock(&m_free_pages_secondary_mutex);//give control back to EvictWorkers to modify free_secondary_list
            }else{
              paddrs[pivot] = paddr;
              iswrites[pivot] = iswrite;
              pivot ++;
              lock();
              continue;
            }
          }
          
          pd = m_free_pages.back();
          m_free_pages.pop_back();
          m_free_page_size -= region_page_ratio;
          pd->page = paddr;
          pd->region = rd;
          pd->set_state_filling();
          pd->dirty = iswrite;
          pd->data_present = false;
          m_busy_pages.push_front(pd);
          m_busy_page_size += region_page_ratio;
          rd->insert_page_descriptor(pd);
        }
                
        WorkItem work;
        work.type = Umap::WorkItem::WorkType::NONE;
        work.page_desc = pd;
        m_rm.get_fill_workers_h()->send_work(work);
         
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
          m_rm.get_fill_workers_h()->send_work(work);
        }
      }
      // There is a chance that the state of this page is not/no-longer
      // PRESENT.  If this is the case, we need to wait for it to finish
      // with whatever is happening to it and then check again
      //
      else{ //in transient status
        //UMAP_LOG(Info, "Add to pivot " <<pivot<< " iswrite = "<< iswrite << " ELSE: " << pd );
        if ( pd->state == PageDescriptor::State::FILLING ){
          //continue;
        }else if ( pd->state == PageDescriptor::State::UPDATING ){
          continue;
        }else if ( pd->state == PageDescriptor::State::LEAVING ){
          //critical to put into pivot, or, deadlock
        }else if ( pd->state == PageDescriptor::State::FREE ){
          //critical to put into pivot, or, deadlock
        }
        paddrs[pivot] = paddr;
        iswrites[pivot] = iswrite;
        pivot ++;
      }
    }
    remaining_pages = pivot;
  }

  unlock();

  UMAP_LOG(Debug, "finish processing " << num_pages << " events");

  // Kick the eviction daemon if the high water mark has been reached
  //
  if ( m_busy_page_size >= m_evict_high_water ) {
    WorkItem w;
    w.type = Umap::WorkItem::WorkType::THRESHOLD;
    w.page_desc = nullptr;
    m_rm.get_evict_manager()->send_work(w);
  }

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

void Buffer::adjust_hi_lo_watermark(){
  
  uint64_t m_size = m_free_page_size + m_free_page_secondary_size + m_busy_page_size;

  m_evict_low_water  = apply_int_percentage(m_rm.get_evict_low_water_threshold(),  m_size);
  m_evict_high_water = apply_int_percentage(m_rm.get_evict_high_water_threshold(), m_size);

  if( m_evict_low_water>=m_evict_high_water ){
    m_evict_high_water = m_size;
    m_evict_low_water  = 1;
  }
}

void Buffer::lock()
{
  int err;
  if ( (err = pthread_mutex_lock(&m_mutex)) != 0 )
    UMAP_ERROR("pthread_mutex_lock failed: " << strerror(err));
}

void Buffer::unlock()
{
  pthread_mutex_unlock(&m_mutex);
}

void Buffer::wait_for_page_present_state( PageDescriptor* pd)
{

  while ( pd->state != PageDescriptor::State::PRESENT ) {
    ++m_waits_for_present_state_change;

    pthread_cond_wait(&m_present_state_change_cond, &m_mutex);

    --m_waits_for_present_state_change;
  }
}

void Buffer::monitor(void)
{
  const int monitor_interval = m_rm.get_monitor_freq();
  UMAP_LOG(Info, "every " << monitor_interval << " seconds");

  /* start the monitoring loop */
  while( is_monitor_on ){

    UMAP_LOG(Info, "Maximum pages in buffer = " << (m_free_page_size+m_free_page_secondary_size+m_busy_page_size)
	     << ", num_busy_pages = " << m_busy_pages.size()
	     << ", num_free_pages = " << m_free_pages.size()
	     << ", events_processed = " << m_stats.events_processed );

    sleep(monitor_interval);

  }//End of loop

}

Buffer::Buffer( RegionManager& rm )
  :   m_rm(rm)
      , m_waits_for_avail_pd(0)
      , m_waits_for_present_state_change(0)
      , m_waits_for_free_state_change(0)
      , m_free_page_size (m_rm.get_max_pages_in_buffer())
      , m_busy_page_size (0)
      , m_psize (m_rm.get_umap_page_size())
{
  m_array = (PageDescriptor *)calloc(m_free_page_size, sizeof(PageDescriptor));
  if ( m_array == nullptr )
    UMAP_ERROR("Failed to allocate " << m_free_page_size*sizeof(PageDescriptor)
        << " bytes for buffer page descriptors");

  for ( int i = 0; i < m_free_page_size; ++i )
    m_free_pages.push_back(&m_array[i]);

  pthread_mutex_init(&m_mutex, NULL);
  pthread_cond_init(&m_avail_pd_cond, NULL);
  pthread_cond_init(&m_present_state_change_cond, NULL);
  pthread_mutex_init(&m_free_pages_secondary_mutex, NULL);
  pthread_cond_init(&m_free_state_change_cond, NULL);

  m_free_page_secondary_size.store(0);
  adjust_hi_lo_watermark();

  /* start up the monitoring thread if requested */
  if( m_rm.get_monitor_freq()>0 ){
    is_monitor_on = true;
    int ret = pthread_create( &monitorThread, NULL, MonitorThreadEntryFunc, this);
    if (ret) {
      UMAP_ERROR("Failed to launch the monitor thread");
    }
  }else{
    is_monitor_on = false;
  }
  UMAP_LOG(Debug, this);
}

Buffer::~Buffer( void ) {
#ifdef UMAP_DISPLAY_STATS
  std::cout << m_stats << std::endl;
#endif

  if( is_monitor_on ){
    is_monitor_on = false;
    pthread_join( monitorThread , NULL );
  }
  
  assert("Pages are still present" && m_busy_pages.size() == 0);
  pthread_cond_destroy(&m_avail_pd_cond);
  pthread_cond_destroy(&m_present_state_change_cond);
  pthread_cond_destroy(&m_free_state_change_cond);
  pthread_mutex_destroy(&m_mutex);
  pthread_mutex_destroy(&m_free_pages_secondary_mutex);
  free(m_array);
}

std::ostream& operator<<(std::ostream& os, const Umap::Buffer* b)
{
  if ( b != nullptr ) {
    os << "{ Maximum pages in buffer: " << (b->m_free_page_size+b->m_free_page_secondary_size+b->m_busy_page_size)
      << ", m_waits_for_avail_pd: " << b->m_waits_for_avail_pd
      << ", m_free_pages.size(): " << std::setw(2) << b->m_free_pages.size()
      << ", m_free_page_size (in unit of global umap page): "    << std::setw(2) << b->m_free_page_size
      << ", m_free_page_secondary_size (in unit of global umap page): " << std::setw(2) << b->m_free_page_secondary_size
      << ", m_busy_pages.size(): " << std::setw(2) << b->m_busy_pages.size()
      << ", m_busy_page_size (in unit of global umap page): "    << std::setw(2) << b->m_busy_page_size
      << ", m_evict_low_water (in unit of global umap page): "   << std::setw(2) << b->m_evict_low_water
      << ", m_evict_high_water (in unit of global umap page): "  << std::setw(2) << b->m_evict_high_water
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
