//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef UMAP_Logger_HPP
#define UMAP_Logger_HPP

#include <string>

namespace Umap {

namespace message {
enum Level {
  Error,
  Warning,
  Info,
  Debug,

  Num_Levels
};

static const std::string MessageLevelName[ Level::Num_Levels ] = {
  "ERROR",
  "WARNING",
  "INFO",
  "DEBUG"
};
} /* namespace messge */

class Logger {
  public:

  void setLoggingMsgLevel( message::Level level ) noexcept;

  void logMessage( message::Level level,
                   const std::string& message,
                   const std::string& fileName,
                   int line ) noexcept;

  static void initialize();

  static void finalize();

  static Logger* getActiveLogger();

  static Logger* getRootLogger();

  inline bool logLevelEnabled( message::Level level )
  {
    if ( level < 0 || level >= message::Num_Levels || m_isEnabled[ level ] == false  )
      return false;
    else
      return true;
  };

private:
  Logger( bool log_with_timestamp ) noexcept;
  ~Logger() noexcept;

  bool m_log_timestamp;
  bool m_isEnabled[ message::Num_Levels ];
  static Logger* s_Logger;
};

} /* namespace Umap */

#endif /* UMAP_Logger_HPP */
