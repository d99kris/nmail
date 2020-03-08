// imap.h
//
// Copyright (c) 2019-2020 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <mutex>
#include <set>
#include <string>

#include "body.h"
#include "header.h"

class Imap
{
public:
  Imap(const std::string& p_User, const std::string& p_Pass, const std::string& p_Host,
       const uint16_t p_Port, const bool p_CacheEncrypt);
  virtual ~Imap();
  
  bool Login();
  bool Logout();

  bool GetFolders(const bool p_Cached, std::set<std::string>& p_Folders);
  bool GetUids(const std::string& p_Folder, const bool p_Cached, std::set<uint32_t>& p_Uids);
  bool GetHeaders(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                  const bool p_Cached, const bool p_Prefetch,
                  std::map<uint32_t, Header>& p_Headers);
  bool GetFlags(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                const bool p_Cached, std::map<uint32_t, uint32_t>& p_Flags);
  bool GetBodys(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                const bool p_Cached, const bool p_Prefetch, std::map<uint32_t, Body>& p_Bodys);

  bool SetFlagSeen(const std::string& p_Folder, const std::set<uint32_t>& p_Uids, bool p_Value);
  bool SetFlagDeleted(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                      bool p_Value);
  bool MoveMessages(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                    const std::string& p_DestFolder);
  bool DeleteMessages(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);
  bool CheckConnection();

  bool GetConnected();
  int IdleStart(const std::string& p_Folder);
  void IdleDone();
  bool UploadMessage(const std::string& p_Folder, const std::string& p_Msg, bool p_IsDraft);

private:
  bool SelectFolder(const std::string& p_Folder, bool p_Force = false);
  bool SelectedFolderIsEmpty();
  uint32_t GetUidValidity();
  std::string GetCacheDir();
  void InitCacheDir();
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

  void InitFolderCacheDir(const std::string& p_Folder);
  void CommonInitCacheDir(const std::string& p_Dir, int p_Version);

  std::string ReadCacheFile(const std::string& p_Path);
  void WriteCacheFile(const std::string& p_Path, const std::string& p_Str);

  void DeleteCacheExceptUids(const std::string &p_Folder, const std::set<uint32_t>& p_Uids);

  static void Logger(struct mailimap* p_Imap, int p_LogType, const char* p_Buffer, size_t p_Size, void* p_UserData);

private:
  std::string m_User;
  std::string m_Pass;
  std::string m_Host;
  uint16_t m_Port = 0;
  bool m_CacheEncrypt = false;

  std::mutex m_ImapMutex;
  struct mailimap* m_Imap = NULL;

  std::mutex m_CacheMutex;

  std::string m_SelectedFolder;
  bool m_SelectedFolderIsEmpty = true;

  std::mutex m_ConnectedMutex;
  bool m_Connected = false;
};
