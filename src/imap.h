// imap.h
//
// Copyright (c) 2019-2025 Kristofer Berggren
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
  struct FolderInfo
  {
    bool IsValid() const
    {
      return ((m_Count != -1) && (m_NextUid != -1) && (m_Unseen != -1));
    }

    bool IsUidsEqual(const FolderInfo& p_Other) const
    {
      return ((m_Count == p_Other.m_Count) && (m_NextUid == p_Other.m_NextUid));
    }

    bool IsUnseenEqual(const FolderInfo& p_Other) const
    {
      return (m_Unseen == p_Other.m_Unseen);
    }

    int32_t m_Count = -1;
    int32_t m_NextUid = -1;
    int32_t m_Unseen = -1;
  };

public:
  Imap(const std::string& p_User, const std::string& p_Pass, const std::string& p_Host,
       const uint16_t p_Port, const int64_t p_Timeout,
       const bool p_CacheEncrypt, const bool p_CacheIndexEncrypt,
       const std::set<std::string>& p_FoldersExclude,
       const bool p_SniEnabled,
       const std::function<void(const StatusUpdate&)>& p_StatusHandler);
  virtual ~Imap();

  bool Login();
  bool Logout();
  bool AuthRefresh();

  bool GetFolders(const bool p_Cached, std::set<std::string>& p_Folders);
  bool GetUids(const std::string& p_Folder, const bool p_Cached, std::set<uint32_t>& p_Uids,
               bool& p_UidInvalid);
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
  bool CopyMessages(const std::string& p_Folder, const std::set<uint32_t>& p_Uids,
                    const std::string& p_DestFolder);
  bool DeleteMessages(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);
  bool CheckConnection();

  bool GetConnected();
  int IdleStart(const std::string& p_Folder);
  bool IdleDone();
  bool UploadMessage(const std::string& p_Folder, const std::string& p_Msg, bool p_IsDraft);

  bool SearchLocal(const std::string& p_QueryStr, const unsigned p_Offset, const unsigned p_Max,
                   std::vector<Header>& p_Headers, std::vector<std::pair<std::string, uint32_t>>& p_FolderUids,
                   bool& p_HasMore);
  bool SearchServer(const std::string& p_QueryStr, const std::string& p_Folder, const unsigned p_Offset,
                    const unsigned p_Max, std::vector<Header>& p_Headers,
                    std::vector<std::pair<std::string, uint32_t>>& p_FolderUids, bool& p_HasMore);

  void SetAborting(bool p_Aborting);
  void IndexNotifyIdle(bool p_IsIdle);

  bool SetBodysCache(const std::string& p_Folder, const std::map<uint32_t, Body>& p_Bodys);

  FolderInfo GetFolderInfo(const std::string& p_Folder);

private:
  bool SelectFolder(const std::string& p_Folder, bool p_Force = false);
  bool SelectedFolderIsEmpty();
  uint32_t GetUidValidity();
  void InitImap();
  void CleanupImap();

  std::set<std::string>& GetCapabilities();
  bool HasCapability(const std::string& p_Name);

  static std::string DecodeFolderName(const std::string& p_Folder);
  static std::string EncodeFolderName(const std::string& p_Folder);

  static std::vector<std::string> SplitQuery(const std::string& p_QueryStr);
  static std::vector<struct mailimap_search_key*> SearchKeysFromQuery(const std::string& p_QueryStr);

  static void Logger(struct mailimap* p_Imap, int p_LogType, const char* p_Buffer, size_t p_Size, void* p_UserData);

private:
  std::string m_User;
  std::string m_Pass;
  std::string m_Host;
  uint16_t m_Port = 0;
  int64_t m_Timeout = 0;
  bool m_CacheEncrypt = false;
  bool m_CacheIndexEncrypt = false;
  std::set<std::string> m_FoldersExclude;
  std::set<std::string> m_UidInvalidFolders;
  bool m_SniEnabled = false;

  std::mutex m_ImapMutex;
  struct mailimap* m_Imap = NULL;

  std::string m_SelectedFolder;
  bool m_SelectedFolderIsEmpty = true;

  std::mutex m_ConnectedMutex;
  bool m_Connected = false;
  bool m_Aborting = false;

  std::string m_LastSearchQueryStr;
  std::string m_LastSearchFolder;
  std::vector<uint32_t> m_LastSearchUids;

  std::shared_ptr<std::set<std::string>> m_Capabilities;

  std::shared_ptr<ImapCache> m_ImapCache;
  std::unique_ptr<ImapIndex> m_ImapIndex;
};
