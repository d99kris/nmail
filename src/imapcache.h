// imapcache.h
//
// Copyright (c) 2020-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <set>
#include <string>

class ImapCache
{
public:
  ImapCache(const bool p_CacheEncrypt, const std::string& p_Pass);
  virtual ~ImapCache();

  std::string GetImapCacheDir();
  void InitImapCacheDir();
  std::string GetFolderCacheDir(const std::string& p_Folder);
  std::string GetFolderUidsCachePath(const std::string& p_Folder);
  std::string GetFolderFlagsCachePath(const std::string& p_Folder);
  std::string GetFoldersCachePath();
  std::string GetMessageCachePath(const std::string& p_Folder, uint32_t p_Uid,
                                  const std::string& p_Suffix);
  std::string GetHeaderCachePath(const std::string& p_Folder, uint32_t p_Uid);
  std::string GetBodyCachePath(const std::string& p_Folder, uint32_t p_Uid);

  bool InitFolderCacheDir(const std::string& p_Folder, int p_UidValidity);

  std::string ReadCacheFile(const std::string& p_Path);
  void WriteCacheFile(const std::string& p_Path, const std::string& p_Str);

  void DeleteCacheExceptUids(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);
  void PruneCacheFolders(const std::set<std::string>& p_KeepFolders);

private:
  bool m_CacheEncrypt;
  std::string m_Pass;
};
