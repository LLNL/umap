//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef _UMAP_Pthread_HPP
#define _UMAP_Pthread_HPP

#include <pthread.h>

namespace Umap {
class Pthread {
public:
   Pthread() {}
   virtual ~Pthread() {}

   // Returns true if the thread was successfully started, false if there was
   // an error starting the thread
   bool StartInternalThread() {
      return (pthread_create(&_thread, NULL, InternalThreadEntryFunc, this) == 0);
   }

   // Will not return until the internal thread has exited.
   void WaitForInternalThreadToExit() {
      (void) pthread_join(_thread, NULL);
   }

protected:
   // Implement this method in your subclass with the code you want your thread
   // to run.
   virtual void InternalThreadEntry() = 0;

private:
   static void*
     InternalThreadEntryFunc(void * This) {
       ((Pthread *)This)->InternalThreadEntry();
       return NULL;
     }

   pthread_t _thread;
};
} // end of namespace Umap

#endif // _UMAP_Pthread_HPP
