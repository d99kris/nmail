// config.h
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>
#include <vector>

#include "util.h"

class Contact
{
public:
  Contact();
  Contact(const std::string& p_Address, const std::string& p_Name = std::string());

  std::string GetAddress() const;
  std::string GetName() const;
  std::string ToString() const;
  static Contact FromString(const std::string& p_Str);
  static std::vector<Contact> FromStrings(const std::vector<std::string>& p_Strs);
  
private:
  std::string m_Address;
  std::string m_Name;
};

std::ostream& operator<<(std::ostream& p_Stream, const Contact& p_Contact);
