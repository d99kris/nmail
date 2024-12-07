// imapmanager.h
//
// Copyright (c) 2019-2024 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>

#include <unistd.h>
#include <sys/ioctl.h>

#include "body.h"
#include "header.h"
#include "imap.h"
#include "log.h"
#include "status.h"

class ImapManager
{
public:
  enum ResponseStatus
  {
    ResponseStatusOk = 0,
    ResponseStatusGetFoldersFailed = (1 << 0),
    ResponseStatusGetUidsFailed = (1 << 1),
    ResponseStatusGetHeadersFailed = (1 << 2),
    ResponseStatusGetFlagsFailed = (1 << 3),
    ResponseStatusGetBodysFailed = (1 << 4),
    ResponseStatusLoginFailed = (1 << 5),
  };

  struct Request
  {
    uint32_t m_PrefetchLevel = 0;
    std::string m_Folder;
    bool m_GetFolders = false;
    bool m_GetUids = false;
    bool m_ProcessHtml = false;
    std::set<uint32_t> m_GetHeaders;
    std::set<uint32_t> m_GetFlags;
    std::set<uint32_t> m_GetBodys;
    uint32_t m_TryCount = 0;
  };

  struct Response
  {
    uint32_t m_ResponseStatus = ResponseStatusOk;
    std::string m_Folder;
    bool m_Cached = false;
    std::set<std::string> m_Folders;
    std::set<uint32_t> m_Uids;
    std::map<uint32_t, Header> m_Headers;
    std::map<uint32_t, uint32_t> m_Flags;
    std::map<uint32_t, Body> m_Bodys;
  };

  struct Action
  {
    std::string m_Folder;
    std::set<uint32_t> m_Uids;
    bool m_SetSeen = false;
    bool m_SetUnseen = false;
    bool m_UploadDraft = false;
    bool m_UploadMessage = false;
    bool m_DeleteMessages = false;
    bool m_UpdateCache = false;
    std::string m_CopyDestination;
    std::string m_MoveDestination;
    std::string m_Msg;
    std::map<uint32_t, Body> m_SetBodysCache;
    uint32_t m_TryCount = 0;
  };

  struct Result
  {
    bool m_Result;
  };

  struct SearchQuery
  {
    std::string m_QueryStr;
    unsigned m_Offset = 0;
    unsigned m_Max = 0;

    SearchQuery()
    {
    }

    SearchQuery(const std::string& p_QueryStr, unsigned p_Offset, unsigned p_Max)
      : m_QueryStr(p_QueryStr)
      , m_Offset(p_Offset)
      , m_Max(p_Max)
    {
    }
  };

  struct SearchResult
  {
    std::vector<Header> m_Headers;
    std::vector<std::pair<std::string, uint32_t>> m_FolderUids;
    bool m_HasMore;
  };

public:
  ImapManager(const std::string& p_User, const std::string& p_Pass, const std::string& p_Host,
              const uint16_t p_Port, const bool p_Connect, const int64_t p_Timeout,
              const bool p_CacheEncrypt,
              const bool p_CacheIndexEncrypt,
              const uint32_t p_IdleTimeout,
              const std::set<std::string>& p_FoldersExclude,
              const bool p_SniEnabled,
              const std::function<void(const ImapManager::Request&, const ImapManager::Response&)>& p_ResponseHandler,
              const std::function<void(const ImapManager::Action&, const ImapManager::Result&)>& p_ResultHandler,
              const std::function<void(const StatusUpdate&)>& p_StatusHandler,
              const std::function<void(const ImapManager::SearchQuery&,
                                       const ImapManager::SearchResult&)>& p_SearchHandler,
              const bool p_IdleInbox,
              const std::string& p_Inbox);
  virtual ~ImapManager();

  void Start();

  void AsyncRequest(const Request& p_Request);
  void PrefetchRequest(const Request& p_Request);
  void AsyncAction(const Action& p_Action);
  void AsyncSearch(const SearchQuery& p_SearchQuery);
  void SyncSearch(const SearchQuery& p_SearchQuery, SearchResult& p_SearchResult);

  void SetCurrentFolder(const std::string& p_Folder);

private:
  struct ProgressCount
  {
    int32_t m_ListTotal = 0;
    int32_t m_ListDone = 0;
    std::unordered_map<std::string, int32_t> m_ItemTotal;
    std::unordered_map<std::string, int32_t> m_ItemDone;
  };

private:
  bool ProcessIdle();
  int GetIdleDurationSec();
  void ProcessIdleOffline();
  void Process();
  bool AuthRefreshNeeded();
  bool PerformAuthRefresh();
  bool CheckConnectivity();
  void CheckConnectivityAndReconnect(bool p_SkipCheck);
  void CacheProcess();
  void SearchProcess();
  bool PerformRequest(const Request& p_Request, bool p_Cached, bool p_Prefetch, Response& p_Response);
  bool PerformAction(const Action& p_Action);
  void PerformSearch(const SearchQuery& p_SearchQuery);
  void SendRequestResponse(const Request& p_Request, const Response& p_Response);
  void SendActionResult(const Action& p_Action, bool p_Result);
  void SetStatus(uint32_t p_Flags, float p_Progress = -1);
  void ClearStatus(uint32_t p_Flags);
  void ProgressCountRequestAdd(const Request& p_Request, bool p_IsPrefetch);
  void ProgressCountRequestDone(const Request& p_Request, bool p_IsPrefetch);
  void ProgressCountReset(bool p_IsPrefetch);
  float GetProgressPercentage(const Request& p_Request, bool p_IsPrefetch);
  void PipeWriteOne(int p_Fds[2]);
  void PipeReadAll(int p_Fds[2]);

private:
  Imap m_Imap;
  bool m_Connect;
  std::function<void(const ImapManager::Request&, const ImapManager::Response&)> m_ResponseHandler;
  std::function<void(const ImapManager::Action&, const ImapManager::Result&)> m_ResultHandler;
  std::function<void(const StatusUpdate&)> m_StatusHandler;
  std::function<void(const SearchQuery&, const SearchResult&)> m_SearchHandler;
  bool m_IdleInbox = true;
  std::string m_Inbox = "";
  uint32_t m_IdleTimeout = 29;
  std::atomic<bool> m_Connecting;
  std::atomic<bool> m_Running;
  std::atomic<bool> m_CacheRunning;
  std::atomic<bool> m_Aborting;
  std::thread m_Thread;
  std::thread m_CacheThread;
  pthread_t m_ThreadId;

  std::deque<Request> m_Requests;
  std::deque<Request> m_CacheRequests;
  std::map<uint32_t, std::deque<Request>> m_PrefetchRequests;
  std::deque<Action> m_Actions;
  ProgressCount m_FetchProgressCount;
  ProgressCount m_PrefetchProgressCount;
  std::mutex m_QueueMutex;
  std::mutex m_CacheQueueMutex;

  std::condition_variable m_ExitedCond;
  std::mutex m_ExitedCondMutex;

  std::condition_variable m_ExitedCacheCond;
  std::mutex m_ExitedCacheCondMutex;

  std::string m_CurrentFolder = "INBOX";
  std::mutex m_Mutex;

  int m_Pipe[2] = { -1, -1 };
  int m_CachePipe[2] = { -1, -1 };

  std::thread m_SearchThread;
  bool m_SearchRunning = false;
  std::deque<SearchQuery> m_SearchQueue;
  std::condition_variable m_SearchCond;
  std::mutex m_SearchMutex;

  bool m_OnceConnected = false;
};
