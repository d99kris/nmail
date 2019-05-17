// maphelp.h
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

template <typename T, typename U>
std::map<T, U> operator-(std::map<T, U> p_Lhs, const std::set<T>& p_Rhs)
{
  for(typename std::map<T, U>::iterator it = p_Lhs.begin(); it != p_Lhs.end(); /* inc in loop */)
  {
    it = (p_Rhs.find(it->first) != p_Rhs.end()) ? p_Lhs.erase(it) : std::next(it);
  }
  return p_Lhs;
}

template<typename T, typename U>
std::pair<U, T> FlipPair(const std::pair<T, U> &p)
{
  return std::pair<U,T>(p.second, p.first);
}

template<typename T, typename U>
std::multimap<U, T> FlipMap(const std::map<T, U> &src)
{
  std::multimap<U, T> dst;
  std::transform(src.begin(), src.end(), std::inserter(dst, dst.begin()), FlipPair<T, U>);
  return dst;
}
