// maphelp.h
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

template<typename T, typename U>
std::map<T, U> operator-(std::map<T, U> p_Lhs, const std::set<T>& p_Rhs)
{
  for (typename std::map<T, U>::iterator it = p_Lhs.begin(); it != p_Lhs.end(); /* inc in loop */)
  {
    it = (p_Rhs.find(it->first) != p_Rhs.end()) ? p_Lhs.erase(it) : std::next(it);
  }
  return p_Lhs;
}

template<typename T, typename U>
std::pair<U, T> FlipPair(const std::pair<T, U>& p_Pair)
{
  return std::pair<U, T>(p_Pair.second, p_Pair.first);
}

template<typename T, typename U>
std::multimap<U, T> FlipMap(const std::map<T, U>& p_Map)
{
  std::multimap<U, T> dst;
  std::transform(p_Map.begin(), p_Map.end(), std::inserter(dst, dst.begin()), FlipPair<T, U>);
  return dst;
}

template<typename T, typename U>
std::set<T> MapKey(const std::map<T, U>& p_Map)
{
  std::set<T> dst;
  std::transform(p_Map.begin(), p_Map.end(), std::inserter(dst, dst.end()), [](std::pair<T, U> pair) {
    return pair.first;
  });
  return dst;
}
