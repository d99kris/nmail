// imapcache.h
//
// Copyright (c) 2020-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <sqlite_modern_cpp.h>

class Body;
class Header;

namespace sqlite
{
  class database;
}

class ImapCache
{
private:
  enum DbType
  {
    HeadersDb = 0,
    BodysDb,
  };

  struct DbConnection;

public:
  ImapCache(const bool p_CacheEncrypt, const std::string& p_Pass);
  virtual ~ImapCache();

  static bool ChangePass(const bool p_CacheEncrypt,
                         const std::string& p_OldPass, const std::string& p_NewPass);

  std::set<std::string> GetFolders();
  void SetFolders(const std::set<std::string>& p_Folders);

  std::set<uint32_t> GetUids(const std::string& p_Folder);
  void SetUids(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);

  std::map<uint32_t, Header> GetHeaders(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                                        const bool p_Prefetch);
  void SetHeaders(const std::string& p_Folder, const std::map<uint32_t, Header>& p_Headers);

  std::map<uint32_t, uint32_t> GetFlags(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);
  void SetFlags(const std::string& p_Folder, const std::map<uint32_t, uint32_t>& p_Flags);

  std::map<uint32_t, Body> GetBodys(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                                    const bool p_Prefetch);
  void SetBodys(const std::string& p_Folder, const std::map<uint32_t, Body>& p_Bodys);

  bool CheckUidValidity(const std::string& p_Folder, int p_UidValidity);
  void SetFlagSeen(const std::string& p_Folder, const std::set<uint32_t>& p_Uids, const bool p_Value);

  void ClearFolder(const std::string& p_Folder);

  void DeleteMessages(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);

  bool Export(const std::string& p_Path);

private:
  void InitHeadersCache();
  void CleanupHeadersCache();

  void InitBodysCache();
  void CleanupBodysCache();

  static std::string GetDbTypeName(ImapCache::DbType p_DbType);
  static std::string GetCacheDir(ImapCache::DbType p_DbType);
  static std::string GetCacheDbDir(ImapCache::DbType p_DbType);
  static std::string GetTempDbDir(ImapCache::DbType p_DbType);
  static std::string GetHeadersFoldersPath();

  std::string GetDbName(const std::string& p_Folder);
  std::string GetDbPath(ImapCache::DbType p_DbType, const std::string& p_Folder);
  void WriteDb(ImapCache::DbType p_DbType, const std::string& p_Folder);
  void CreateDb(ImapCache::DbType p_DbType, const std::string& p_DbPath);
  std::shared_ptr<DbConnection> GetDb(DbType p_DbType, const std::string& p_Folder, bool p_Writable);
  void CloseDbs(DbType p_DbType);
  std::string ReadCacheFile(const std::string& p_Path);
  void WriteCacheFile(const std::string& p_Path, const std::string& p_Str);

  void DeleteUids(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);
  void DeleteFlags(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);
  void DeleteHeaders(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);
  void DeleteBodys(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);

private:
  bool m_CacheEncrypt;
  std::string m_Pass;
  std::set<std::string> m_Folders;

  std::mutex m_CacheMutex;
  std::map<DbType, std::map<std::string, std::shared_ptr<DbConnection>>> m_DbConnections;
  std::map<DbType, std::string> m_CurrentWriteDb;
};
