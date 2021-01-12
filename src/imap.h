// imap.h
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>

#include "body.h"
#include "header.h"
#include "imapcache.h"
#include "imapindex.h"

class Imap
{
public:
  Imap(const std::string& p_User, const std::string& p_Pass, const std::string& p_Host,
       const uint16_t p_Port, const bool p_CacheEncrypt, const bool p_CacheIndexEncrypt,
       const std::set<std::string>& p_FoldersExclude,
       const std::function<void(const StatusUpdate&)>& p_StatusHandler);
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

  void Search(const std::string& p_QueryStr, const unsigned p_Offset, const unsigned p_Max,
              std::vector<Header>& p_Headers, std::vector<std::pair<std::string, uint32_t>>& p_FolderUids,
              bool& p_HasMore);

private:
  bool SelectFolder(const std::string& p_Folder, bool p_Force = false);
  bool SelectedFolderIsEmpty();
  bool LockSelectedFolder(bool p_DoLock);
  uint32_t GetUidValidity();

  static void Logger(struct mailimap* p_Imap, int p_LogType, const char* p_Buffer, size_t p_Size, void* p_UserData);

private:
  std::string m_User;
  std::string m_Pass;
  std::string m_Host;
  uint16_t m_Port = 0;
  bool m_CacheEncrypt = false;
  bool m_CacheIndexEncrypt = false;
  std::set<std::string> m_FoldersExclude;

  std::mutex m_ImapMutex;
  struct mailimap* m_Imap = NULL;

  std::mutex m_CacheMutex;

  std::string m_SelectedFolder;
  bool m_SelectedFolderIsEmpty = true;

  std::mutex m_ConnectedMutex;
  bool m_Connected = false;

  std::unique_ptr<ImapCache> m_ImapCache;
  std::unique_ptr<ImapIndex> m_ImapIndex;
};
