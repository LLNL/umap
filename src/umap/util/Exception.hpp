//////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
// UMAP Project Developers. See the top-level LICENSE file for details.
//
// SPDX-License-Identifier: LGPL-2.1-only
//////////////////////////////////////////////////////////////////////////////
#ifndef UMAP_Exception_HPP
#define UMAP_Exception_HPP

#include <string>
#include <exception>

namespace Umap {

class Exception : public std::exception {
  public:
    Exception(const std::string& msg,
        const std::string &file,
        int line);

    virtual ~Exception() = default;

    std::string message() const;
    virtual const char* what() const throw();

  private:
    std::string m_message;
    std::string m_file;
    int m_line;

    std::string m_what;
};

} // end of namespace Umap

#endif // UMAP_Exception_HPP
