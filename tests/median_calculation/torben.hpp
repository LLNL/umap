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
/*
The original code of Torben algorithm is in public domain
Its algorithm is developed by Torben Mogensen and implementated by N. Devillard
This version considerably modified
This implementation also contains the modification proposed in https://github.com/sarnold/medians-1D/issues/8
*/

#ifndef UMAP_TORBEN_HPP
#define UMAP_TORBEN_HPP

#include <algorithm>
#include <iterator>

/*
STL-like Torben algorithm implementation for median calculation
Returns the median value of given elements that are accessible via a random access iterator

\tparam iterator_type Type of the iterator
\param iterator_begin Iterator for the beginning position
\param iterator_end Iterator for the end position
\return Calculated median value. If the size of the array is 0, returns 0.

\example
1) STL container
std::vector<int> vec;
int median = torben(vec.cbegin(), vec.cend());

2) you can also use a normal array
int array[10];
int median = torben(array, array + 10);

3) use your own iterator
own_iterator_class itr_begin;
own_iterator_class itr_end;
int median = torben(itr_begin, itr_end);


class vector_iterator {
 public:
  // Required types to use some stl functions
  using value_type = pixel_type;
  using difference_type = ssize_t;
  using iterator_category = std::random_access_iterator_tag;
  using pointer = value_type *;
  using reference = value_type &;

  // Constructor
  vector_iterator(const median::cube_t<pixel_type> &_cube,
                         const median::vector_t &_vector,
                         const size_t _start_pos)
      : cube(_cube),
        vector(_vector),
        current_pos(_start_pos) {}

  // Copy constructor
  vector_iterator(const vector_iterator&) = default;

  bool operator!=(const vector_iterator &);
  difference_type operator-(const vector_iterator &);
  value_type operator*();
  value_type operator[](size_t);

  // To support
  // ++iterator
  value_type operator++() {
    size_t tmp = current_pos;
    ++current_pos;
    return (*this)[tmp];
  }

  median::cube_t<pixel_type> cube;
  median::vector_t vector;
  size_t current_pos;
};
 */
template <typename iterator_type>
typename iterator_type::value_type
torben(iterator_type iterator_begin, iterator_type iterator_end) {
  using value_type = typename std::iterator_traits<iterator_type>::value_type;

  if (iterator_begin == iterator_end)
    return 0;

  // ---------- Find min and max value over time frame ---------- //
  value_type min = *iterator_begin;
  value_type max = *iterator_begin;
  std::size_t length = 0;
  for (auto iterator(iterator_begin); iterator != iterator_end; ++iterator) {
    const value_type value = *iterator;
    min = std::min(min, value);
    max = std::max(max, value);
    ++length;
  }
  const size_t half = (length + 1) / 2;

  // ---------- Find median value ---------- //
  size_t less, greater, equal;
  value_type guess, maxltguess, mingtguess;
  while (true) {
    guess = (min + max) / 2.0; // Should cast to double before divide?
    less = 0;
    greater = 0;
    equal = 0;
    maxltguess = min;
    mingtguess = max;

    for (auto iterator(iterator_begin); iterator != iterator_end; ++iterator) {
      const value_type value = *iterator;
      if (value < guess) {
        less++;
        if (value > maxltguess) maxltguess = value;
      } else if (value > guess) {
        greater++;
        if (value < mingtguess) mingtguess = value;
      } else {
        equal++;
      }
    }

    if (less <= half && greater <= half) break;
    else if (less > greater) max = maxltguess;
    else min = mingtguess;
  }

  // ----- Calculate a mean value if the number of the given elements is an even number ----- //
  if (less >= half) min = maxltguess;
  else if (less + equal >= half) min = guess;
  else min = mingtguess;

  if (length & 1) return min;

  if (greater >= half) max = mingtguess;
  else if (greater + equal >= half) max = guess;
  else max = maxltguess;

  return (min + max) / 2.0;
}

#endif //UMAP_TORBEN_HPP
