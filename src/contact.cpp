// contact.cpp
//
// Copyright (c) 2019-2020 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "contact.h"

Contact::Contact()
{
}

Contact::Contact(const std::string& p_Address, const std::string& p_Name)
  : m_Address(p_Address)
  , m_Name(p_Name)
{
}

std::string Contact::GetAddress() const
{
  return m_Address;
}

std::string Contact::GetName() const
{
  return m_Name;
}

std::string Contact::ToString() const
{
  if (m_Name.empty())
  {
    return m_Address;
  }
  else
  {
    return m_Name + std::string(" <") + m_Address + std::string(">");
  }
}

Contact Contact::FromString(const std::string& p_Str)
{
  std::string::size_type startBracket = p_Str.find("<");
  if (startBracket != std::string::npos)
  {
    std::string::size_type startAddress = startBracket + 1;
    std::string::size_type endBracket = p_Str.find(">", startAddress);
    if (endBracket != std::string::npos)
    {
      const std::string& addr =
        Util::Trim(p_Str.substr(startAddress, endBracket - startAddress));
      const std::string& name = Util::Trim(p_Str.substr(0, std::max((int)startBracket - 1, 0)));

      return Contact(addr, name);
    }
  }
  return Contact(p_Str);
}

std::vector<Contact> Contact::FromStrings(const std::vector<std::string>& p_Strs)
{
  std::vector<Contact> contacts;
  for (auto& str : p_Strs)
  {
    contacts.push_back(FromString(str));
  }
  return contacts;
}

std::ostream& operator<<(std::ostream& p_Stream, const Contact& p_Contact)
{
  p_Stream << p_Contact.ToString();
  return p_Stream;
}
