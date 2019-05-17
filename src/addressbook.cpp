// addressbook.cpp
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "addressbook.h"

#include <map>
#include <string>
#include <utility>

#include "aes.h"
#include "log.h"
#include "loghelp.h"
#include "maphelp.h"
#include "serialized.h"
#include "util.h"

std::mutex AddressBook::m_Mutex;
bool AddressBook::m_CacheEncrypt = true;
std::string AddressBook::m_Pass;
std::set<std::string> AddressBook::m_MsgIds;
std::map<std::string, uint32_t> AddressBook::m_Addresses;

void AddressBook::Init(const bool p_CacheEncrypt, const std::string& p_Pass)
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  m_CacheEncrypt = p_CacheEncrypt;
  m_Pass = p_Pass;

  InitCacheDir();

  m_MsgIds = Deserialize<std::set<std::string>>(ReadCacheFile(GetMsgIdsCachePath()));
  m_Addresses = Deserialize<std::map<std::string, uint32_t>>(ReadCacheFile(GetAddressesCachePath()));
}

void AddressBook::Cleanup()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  WriteCacheFile(GetMsgIdsCachePath(), Serialize(m_MsgIds));
  WriteCacheFile(GetAddressesCachePath(), Serialize(m_Addresses));
}

void AddressBook::Add(const std::string& p_MsgId, const std::set<std::string>& p_Addresses)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  if (m_MsgIds.find(p_MsgId) == m_MsgIds.end())
  {
    m_MsgIds.insert(p_MsgId);
    for (auto& address : p_Addresses)
    {
      auto foundAddress = m_Addresses.find(address);
      if (foundAddress != m_Addresses.end())
      {
        foundAddress->second = foundAddress->second + 1;
      }
      else
      {
        m_Addresses.insert(std::pair<std::string, uint32_t>(address, 1));
      }      
    }
  }
}

std::vector<std::string> AddressBook::Get()
{
  std::multimap<uint32_t, std::string> sortedAddresses = FlipMap(m_Addresses);
  std::vector<std::string> addresses;

  for (auto it = sortedAddresses.rbegin(); it != sortedAddresses.rend(); ++it)
  {
    addresses.push_back(it->second);
  }
  
  return addresses;
}

void AddressBook::InitCacheDir()
{
  static const int version = 1;
  const std::string cacheDir = GetAddressCacheDir();
  CommonInitCacheDir(cacheDir, version);
}

void AddressBook::CommonInitCacheDir(const std::string &p_Dir, int p_Version)
{
  const std::string& dirVersionPath = p_Dir + "version";
  if (Util::Exists(p_Dir))
  {
    int dirVersion = -1;
    DeserializeFromFile(dirVersionPath, dirVersion);
    if (dirVersion != p_Version)
    {
      Util::RmDir(p_Dir);
      Util::MkDir(p_Dir);
      SerializeToFile(dirVersionPath, p_Version);
    }
  }
  else
  {
    Util::MkDir(p_Dir);
    SerializeToFile(dirVersionPath, p_Version);
  }
}

std::string AddressBook::GetAddressCacheDir()
{
  return Util::GetApplicationDir() + std::string("cache/") + std::string("address/");
}

std::string AddressBook::GetMsgIdsCachePath()
{
  return GetAddressCacheDir() + std::string("msgids");
}

std::string AddressBook::GetAddressesCachePath()
{
  return GetAddressCacheDir() + std::string("addresses");
}

std::string AddressBook::ReadCacheFile(const std::string &p_Path)
{
  if (m_CacheEncrypt)
  {
    return AES::Decrypt(Util::ReadFile(p_Path), m_Pass);
  }
  else
  {
    return Util::ReadFile(p_Path);
  }
}

void AddressBook::WriteCacheFile(const std::string &p_Path, const std::string &p_Str)
{
  if (m_CacheEncrypt)
  {
    Util::WriteFile(p_Path, AES::Encrypt(p_Str, m_Pass));
  }
  else
  {
    Util::WriteFile(p_Path, p_Str);
  }
}
