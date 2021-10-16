// imapcacheindex.h
//
// Copyright (c) 2020-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <condition_variable>
#include <functional>
#include <queue>
#include <set>
#include <string>
#include <thread>

#include "header.h"
#include "imapcache.h"
#include "log.h"
#include "searchengine.h"
#include "status.h"
#include "util.h"

class ImapIndex
{
public:
  explicit ImapIndex(const bool p_CacheIndexEncrypt,
                     const std::string& p_Pass,
                     std::shared_ptr<ImapCache> p_ImapCache,
                     const std::function<void(const StatusUpdate&)>& p_StatusHandler);
  virtual ~ImapIndex();

  static bool ChangePass(const bool p_CacheEncrypt,
                         const std::string& p_OldPass, const std::string& p_NewPass);

  void NotifyIdle(bool p_IsIdle);

  void SetFolders(const std::set<std::string>& p_Folders);
  void SetUids(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);
  void DeleteMessages(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);
  void SetBodys(const std::string& p_Folder, const std::set<uint32_t>& p_Uids);

  void Search(const std::string& p_QueryStr, const unsigned p_Offset, const unsigned p_Max,
              std::vector<Header>& p_Headers, std::vector<std::pair<std::string, uint32_t>>& p_FolderUids,
              bool& p_HasMore);

private:
  struct Notify
  {
    std::set<std::string> m_SetFolders;
    std::string m_Folder;
    std::set<uint32_t> m_SetUids;
    std::set<uint32_t> m_DeleteUids;
    std::set<uint32_t> m_SetBodys;
  };

private:
  void Process();
  void HandleNotify(const Notify& p_Notify);
  void HandleCommit(bool p_ForceCommit);
  void HandleSyncEnqueue();
  void AddMessage(const std::string& p_Folder, uint32_t p_Uid);

  std::string GetDocId(const std::string& p_Folder, const uint32_t p_Uid);
  std::string GetFolderFromDocId(const std::string& p_DocId);
  uint32_t GetUidFromDocId(const std::string& p_DocId);

  static std::string GetCacheIndexDir();
  static std::string GetCacheIndexDbDir();
  static std::string GetCacheIndexDbTempDir();
  void InitCacheIndexDir();
  static void InitCacheTempDir();
  static void CleanupCacheTempDir();

  void SetStatus(uint32_t p_Flags, int32_t p_Progress = -1);
  void ClearStatus(uint32_t p_Flags);

private:
  std::unique_ptr<SearchEngine> m_SearchEngine;
  bool m_CacheIndexEncrypt = false;
  std::string m_Pass;
  std::shared_ptr<ImapCache> m_ImapCache;
  std::function<void(const StatusUpdate&)> m_StatusHandler;
  bool m_Running = false;
  bool m_IsIdle = false;
  std::thread m_Thread;
  std::mutex m_ProcessMutex;
  std::condition_variable m_ProcessCondVar;
  std::queue<Notify> m_Queue;
  size_t m_QueueSize = 0;
  bool m_Dirty = false;
  bool m_SyncDone = false;
};
