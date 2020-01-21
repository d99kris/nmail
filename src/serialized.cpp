// serialized.cpp
//
// Copyright (c) 2019-2020 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "serialized.h"

Serialized::Serialized()
{
}

Serialized::Serialized(const std::string &p_Path)
  : m_Path(p_Path)
{
  Load(p_Path);
}

Serialized::Serialized(const Serialized &p_Serialized)
  : m_String(p_Serialized.m_String)
{
}

void Serialized::Clear()
{
  m_String.clear();
}

void Serialized::FromString(const std::string &p_String)
{
  m_String = p_String;
}

std::string Serialized::ToString()
{
  return m_String;
}

void Serialized::Load(const std::string &p_Path)
{
  std::ifstream file(p_Path);
  std::stringstream ss;
  ss << file.rdbuf();
  m_String = ss.str();
}

void Serialized::Save(const std::string &p_Path)
{
  const std::string& path = !p_Path.empty() ? p_Path : m_Path;
  std::ofstream file(path);
  file << m_String;
}

std::string Serialized::ToHex(const std::string &p_String)
{
  std::ostringstream oss;
  for(const char& ch : p_String)
  {
    char buf[3] = { 0 };
    snprintf(buf, sizeof(buf), "%02X", (unsigned char)ch);
    oss << buf;
  }

  return oss.str();
}

std::string Serialized::FromHex(const std::string &p_String)
{
  std::string result;
  std::istringstream iss(p_String);
  char buf[3] = { 0 };
  while (iss.read(buf, 2))
  {
    result += static_cast<char>(strtol(buf, NULL, 16) & 0xff);
  }

  return result;
}
