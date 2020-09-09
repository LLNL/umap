//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#include "umap/util/Exception.hpp"

#include <sstream>

namespace Umap {

Exception::Exception(
    const std::string& message,
    const std::string &file,
    int line) :
  m_message(message),
  m_file(file),
  m_line(line)
{
  m_what = this->message();
}

std::string
Exception::message() const
{
  std::stringstream oss;
  oss << "! UMAP Exception [" << m_file << ":" << m_line << "]: ";
  oss << m_message;
  return oss.str();
}

const char*
Exception::what() const throw()
{
  return m_what.c_str();
}

} // end of namespace Umap
