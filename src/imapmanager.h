// imapmanager.h
//
// Copyright (c) 2019 Kristofer Berggren
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
  };
  
  struct Request
  {
    bool m_Prefetch = false;
    std::string m_Folder;
    bool m_GetFolders = false;
    bool m_GetUids = false;
    std::set<uint32_t> m_GetHeaders;
    std::set<uint32_t> m_GetFlags;
    std::set<uint32_t> m_GetBodys;
  };

  struct Response
  {
    uint32_t m_ResponseStatus = ResponseStatusOk;
    std::string m_Folder;
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
    std::string m_MoveDestination;
  };

  struct Result
  {
    bool m_Result;
  };

public:
  ImapManager(const std::string& p_User, const std::string& p_Pass, const std::string& p_Host,
              const uint16_t p_Port, const bool p_Connect, const bool p_CacheEncrypt,
              const std::function<void(const ImapManager::Request&,const ImapManager::Response&)>& p_ResponseHandler,
              const std::function<void(const ImapManager::Action&,const ImapManager::Result&)>& p_ResultHandler,
              const std::function<void(const StatusUpdate&)>& p_StatusHandler);
  virtual ~ImapManager();

  void AsyncRequest(const Request& p_Request);
  void PrefetchRequest(const Request& p_Request);
  void AsyncAction(const Action& p_Action);
  void SetCurrentFolder(const std::string& p_Folder);
  
private:
  void ProcessIdle();
  void Process();
  void PerformRequest(const Request& p_Request, bool p_Cached);
  void PerformAction(const Action& p_Action);
  void SetStatus(uint32_t p_Flags);
  void ClearStatus(uint32_t p_Flags);

private:
  Imap m_Imap;
  bool m_Connect;
  std::function<void(const ImapManager::Request&,const ImapManager::Response&)> m_ResponseHandler;
  std::function<void(const ImapManager::Action&,const ImapManager::Result&)> m_ResultHandler;
  std::function<void(const StatusUpdate&)> m_StatusHandler;
  std::atomic<bool> m_Connecting;
  std::atomic<bool> m_Running;
  std::thread m_Thread;
  
  std::deque<Request> m_Requests;
  std::deque<Request> m_PrefetchRequests;
  std::deque<Action> m_Actions;
  std::mutex m_QueueMutex;

  std::condition_variable m_ExitedCond;
  std::mutex m_ExitedCondMutex;

  std::string m_CurrentFolder = "INBOX";
  std::mutex m_Mutex;

  int m_Pipe[2] = {-1, -1};
};
