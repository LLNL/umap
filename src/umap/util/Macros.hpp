//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef UMAP_Macros_HPP
#define UMAP_Macros_HPP

#include "umap/util/Exception.hpp"
#include "umap/config.h"

#include <sstream>
#include <iostream>

#ifdef UMAP_ENABLE_ASSERTS
#include <cassert>
#define UMAP_ASSERT(condition) assert(condition)
#else
#define UMAP_ASSERT(condition) ((void)0)
#endif // UMAP_ENABLE_ASSERTS

#ifdef UMAP_DEBUG_LOGGING

#include "umap/util/Logger.hpp"
#define UMAP_LOG( lvl, msg )                                                  \
{                                                                             \
  if (Umap::Logger::getActiveLogger()->logLevelEnabled(Umap::message::lvl)) { \
    std::ostringstream local_msg;                                             \
    local_msg  << " " << __func__ << " " << msg;                              \
    Umap::Logger::getActiveLogger()->logMessage(                              \
        Umap::message::lvl, local_msg.str(),                                  \
        std::string(__FILE__), __LINE__);                                     \
  }                                                                           \
}

#else

#define UMAP_LOG( lvl, msg ) ((void)0)

#endif // UMAP_DEBUG_LOGGING

#define UMAP_UNUSED_ARG(x)

#define UMAP_USE_VAR(x) static_cast<void>(x)

#define UMAP_ERROR( msg )                                        \
{                                                                \
  UMAP_LOG(Error, msg);                                          \
  std::ostringstream umap_oss_error;                             \
  umap_oss_error << " " << __func__ << " " << msg;               \
  throw Umap::Exception( umap_oss_error.str(),                   \
                                 std::string(__FILE__),          \
                                 __LINE__);                      \
}

#endif // UMAP_Macros_HPP
