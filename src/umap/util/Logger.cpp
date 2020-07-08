//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include "umap/util/Logger.hpp"

#include <iostream>   // for std::cout, std::cerr
#include <mutex>
#include <stdlib.h>   // for getenv()
#include <strings.h>  // for strcasecmp()
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace Umap {

static const char* env_name = "UMAP_LOG_LEVEL";
static const char* env_name_no_timestamp = "UMAP_LOG_NO_TIMESTAMP_LEVEL";
static message::Level defaultLevel = message::Debug;
std::mutex g_logging_mutex;
Logger* Logger::s_Logger = nullptr;

static const std::string MessageLevelName[ message::Num_Levels ] = {
  "ERROR",
  "WARNING",
  "INFO",
  "DEBUG"
};

Logger::Logger(bool log_with_timestamp) noexcept
  : m_log_timestamp(log_with_timestamp)
{
  // by default, all message streams are disabled
  for ( int i=0 ; i < message::Num_Levels ; ++i )
    m_isEnabled[ i ] = false;
}

Logger::~Logger() noexcept
{
}

void Logger::setLoggingMsgLevel( message::Level level ) noexcept
{
  for ( int i=0 ; i < message::Num_Levels ; ++i )
    m_isEnabled[ i ] = (i<= level) ? true : false;
}

void Logger::logMessage( message::Level level,
                         const std::string& message,
                         const std::string& fileName,
                         int line ) noexcept
{
  if ( !logLevelEnabled( level ) )
    return;   /* short-circuit */

  std::lock_guard<std::mutex> guard(g_logging_mutex);
  if (m_log_timestamp) {
    std::cout
      << getpid() << ":"
      << syscall(__NR_gettid) << " "
      << "[" << MessageLevelName[ level ] << "]"
      << "[" << fileName  << ":" << line << "]:"
      << message
      << std::endl;
  }
  else {
    std::cout
      << message
      << std::endl;
  }
}

void Logger::initialize()
{
  if ( s_Logger != nullptr )
    return;

  message::Level level = defaultLevel;
  char* enval = getenv(env_name);
  char* enval_no_timestamp = getenv(env_name_no_timestamp);
  bool log_with_timestamp = true;

  if ( enval != NULL || enval_no_timestamp != NULL ) {

    if (enval_no_timestamp != NULL) {
      enval = enval_no_timestamp;
      log_with_timestamp = false;
    }

    bool level_found = false;
    for ( int i = 0; i < message::Num_Levels; ++i ) {
      if ( strcasecmp( enval, MessageLevelName[ i ].c_str() ) == 0 ) {
        level_found = true;
        level = (message::Level)i;
        break;
      }
    }
    if (! level_found ) {
      std::cerr << "No matching logging levels for: " << enval << "\n";
      std::cerr << "Available levels are:\n";
      for ( int i = 0; i < message::Num_Levels; ++i ) {
        std::cerr << "\t" << MessageLevelName[ i ] << "\n";
      }
    }
  }

  s_Logger = new Logger(log_with_timestamp);
  s_Logger->setLoggingMsgLevel(level);
}

void Logger::finalize()
{
  delete s_Logger;
  s_Logger = nullptr;
}

Logger* Logger::getActiveLogger()
{
  if ( s_Logger == nullptr )
    Logger::initialize();

  return s_Logger;
}

} /* namespace umap */
