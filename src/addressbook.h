// addressbook.h
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <algorithm>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

class AddressBook
{
public:
  static void Init(const bool p_CacheEncrypt, const std::string& p_Pass);
  static void Cleanup();
  
  static void Add(const std::string& p_MsgId, const std::set<std::string>& p_Addresses);
  static std::vector<std::string> Get();

private:
  static void InitCacheDir();
  static void CommonInitCacheDir(const std::string &p_Dir, int p_Version);
  static std::string GetAddressCacheDir();
  static std::string GetMsgIdsCachePath();
  static std::string GetAddressesCachePath();
  static std::string ReadCacheFile(const std::string &p_Path);
  static void WriteCacheFile(const std::string &p_Path, const std::string &p_Str);

private:
  static std::mutex m_Mutex;
  static bool m_CacheEncrypt;
  static std::string m_Pass;
  static std::set<std::string> m_MsgIds;
  static std::map<std::string, uint32_t> m_Addresses;
};
