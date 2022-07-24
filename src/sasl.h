// sasl.h
//
// Copyright (c) 2020-2022 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <set>
#include <string>

class Sasl
{
public:
  static std::string GetMechanismsStr();
  static bool IsMechanismsSupported(int p_Auths);

private:
  static std::set<std::string> GetMechanisms();
  static bool IsMechanismSupported(int p_Auths, int p_Auth, const std::string& p_AuthStr);
  static bool IsRequestedMechanismSupported(int p_Auths, int p_ReqAuth, const std::string& p_AuthStr);
};
