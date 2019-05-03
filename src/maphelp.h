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
