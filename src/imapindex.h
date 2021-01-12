// imapcacheindex.h
//
// Copyright (c) 2020-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <condition_variable>
#include <functional>
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
  explicit ImapIndex(const bool p_CacheIndexEncrypt, const bool p_CacheEncrypt,
                     const std::string& p_Pass,
                     std::set<std::string>& p_Folders,
                     const std::function<void(const StatusUpdate&)>& p_StatusHandler);
  virtual ~ImapIndex();

  void NotifyIdle(bool p_IsIdle);
  void EnqueueSyncFolders(std::set<std::string>& p_Folders);
  void EnqueueAddFolder(const std::string& p_Folder);
  void EnqueueDeleteFolder(const std::string& p_Folder);
  void EnqueueAddMessage(const std::string& p_Folder, uint32_t p_Uid);
  void EnqueueDeleteMessages(const std::string& p_Folder, const std::set<uint32_t>& p_KeepUids);
  void Search(const std::string& p_QueryStr, const unsigned p_Offset, const unsigned p_Max,
              std::vector<Header>& p_Headers, std::vector<std::pair<std::string, uint32_t>>& p_FolderUids,
              bool& p_HasMore);

private:
  void Process();
  void AddFolder(const std::string& p_Folder);
  void DeleteFolder(const std::string& p_Folder);
  void AddMessage(const std::string& p_Folder, uint32_t p_Uid);
  void DeleteMessages(const std::string& p_Folder, const std::set<uint32_t>& p_KeepUids);
  std::string GetDocId(const std::string& p_Folder, const uint32_t p_Uid);
  std::string GetFolderFromDocId(const std::string& p_DocId);
  uint32_t GetUidFromDocId(const std::string& p_DocId);
  std::string GetCacheIndexDir();
  std::string GetCacheIndexDbDir();
  std::string GetCacheIndexDbTempDir();
  void InitCacheIndexDir();
  void InitCacheTempDir();
  void CleanupCacheTempDir();
  void SetStatus(uint32_t p_Flags, int32_t p_Progress = -1);
  void ClearStatus(uint32_t p_Flags);

private:
  std::unique_ptr<SearchEngine> m_SearchEngine;
  std::unique_ptr<ImapCache> m_ImapCache;
  bool m_CacheIndexEncrypt = false;
  std::string m_Pass;
  std::set<std::string> m_AddedFolders;
  std::function<void(const StatusUpdate&)> m_StatusHandler;
  bool m_Running = false;
  bool m_IsIdle = false;
  std::thread m_Thread;
  std::mutex m_ProcessMutex;
  std::condition_variable m_ProcessCondVar;
  std::map<std::string, std::set<uint32_t>> m_AddQueue;
  std::map<std::string, std::set<uint32_t>> m_DeleteQueue;
  size_t m_QueueSize = 0;
};
