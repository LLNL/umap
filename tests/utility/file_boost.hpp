/*
This file is part of UMAP.  For copyright information see the COPYRIGHT
file in the top level directory, or at
https://github.com/LLNL/umap/blob/master/COPYRIGHT
This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free
Software Foundation) version 2.1 dated February 1999.  This program is
distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the terms and conditions of the GNU Lesser General Public License
for more details.  You should have received a copy of the GNU Lesser General
Public License along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/


#ifndef UTILITY_FILE_BOOST_HPP
#define UTILITY_FILE_BOOST_HPP

#include <iostream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

namespace utility
{

/// \brief If the given path is a regular file, just return it
/// \param path Path to a regular file or directory
/// \param filenames Found files are inserted in here
/// \param extension Extension of target files.
void get_filenames(const std::string &path, std::vector<std::string> &filenames, const std::string extension = "")
{
  const boost::filesystem::path target_path(path);

  /// If the given path is a regular file, just return it if its extension matches.
  if (boost::filesystem::is_regular_file(target_path)) {
    if (extension == "" || target_path.filename().extension().generic_string() == extension)
      filenames.push_back(path);
    return;
  }

  /// List all files in the directory which matches to the given extension
  for (const auto &e : boost::make_iterator_range(boost::filesystem::directory_iterator(target_path), {})) {
    if (!boost::filesystem::is_directory(e)) {
      if (extension == "" || e.path().filename().extension().generic_string() == extension) {
        filenames.push_back(e.path().generic_string());
      }
    }
  }

}

}// namespace utility
#endif //UTILITY_FILE_BOOST_HPP
