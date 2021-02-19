// imapmanager.cpp
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "imapmanager.h"

#include <vector>

#include "loghelp.h"
#include "serialized.h"
#include "util.h"

ImapManager::ImapManager(const std::string& p_User, const std::string& p_Pass,
                         const std::string& p_Host, const uint16_t p_Port,
                         const bool p_Connect, const int64_t p_Timeout,
                         const bool p_CacheEncrypt,
                         const bool p_CacheIndexEncrypt,
                         const std::set<std::string>& p_FoldersExclude,
                         const std::function<void(const ImapManager::Request&,
                                                  const ImapManager::Response&)>& p_ResponseHandler,
                         const std::function<void(const ImapManager::Action&,
                                                  const ImapManager::Result&)>& p_ResultHandler,
                         const std::function<void(const StatusUpdate&)>& p_StatusHandler,
                         const std::function<void(const SearchQuery&,
                                                  const SearchResult&)>& p_SearchHandler)
  : m_Imap(p_User, p_Pass, p_Host, p_Port, p_Timeout,
           p_CacheEncrypt, p_CacheIndexEncrypt, p_FoldersExclude, p_StatusHandler)
  , m_Connect(p_Connect)
  , m_ResponseHandler(p_ResponseHandler)
  , m_ResultHandler(p_ResultHandler)
  , m_StatusHandler(p_StatusHandler)
  , m_SearchHandler(p_SearchHandler)
  , m_Connecting(false)
  , m_Running(false)
  , m_CacheRunning(false)
{
  pipe(m_Pipe);
  pipe(m_CachePipe);
  m_Connecting = m_Connect;
}

ImapManager::~ImapManager()
{
  LOG_DEBUG("stop threads");
  {
    std::unique_lock<std::mutex> lock(m_ExitedCondMutex);

    if (m_QueueMutex.try_lock())
    {
      m_Requests.clear();
      m_PrefetchRequests.clear();
      m_Actions.clear();
      m_QueueMutex.unlock();
      LOG_DEBUG("queues cleared");
    }
    else
    {
      LOG_DEBUG("queues not cleared");
    }

    m_Running = false;
    write(m_Pipe[1], "1", 1);

    if (m_ExitedCond.wait_for(lock, std::chrono::seconds(5)) != std::cv_status::timeout)
    {
      m_Thread.join();
      LOG_DEBUG("process thread joined");
    }
    else
    {
      LOG_WARNING("process thread exit timeout");
    }
  }

  {
    std::unique_lock<std::mutex> lock(m_ExitedCacheCondMutex);

    m_CacheRunning = false;
    write(m_CachePipe[1], "1", 1);

    if (m_ExitedCacheCond.wait_for(lock, std::chrono::seconds(2)) != std::cv_status::timeout)
    {
      m_CacheThread.join();
      LOG_DEBUG("cache thread joined");
    }
    else
    {
      LOG_WARNING("cache thread exit timeout");
    }
  }

  if (m_SearchRunning)
  {
    std::unique_lock<std::mutex> lock(m_SearchMutex);
    m_SearchRunning = false;
    m_SearchCond.notify_one();
  }

  if (m_SearchThread.joinable())
  {
    m_SearchThread.join();
  }

  close(m_Pipe[0]);
  close(m_Pipe[1]);
  close(m_CachePipe[0]);
  close(m_CachePipe[1]);
}

void ImapManager::Start()
{
  SetStatus(m_Connecting ? Status::FlagConnecting : Status::FlagOffline);
  m_Running = true;
  m_CacheRunning = true;
  m_SearchRunning = true;
  LOG_DEBUG("start threads");
  m_Thread = std::thread(&ImapManager::Process, this);
  m_CacheThread = std::thread(&ImapManager::CacheProcess, this);
  m_SearchThread = std::thread(&ImapManager::SearchProcess, this);
}

void ImapManager::AsyncRequest(const ImapManager::Request& p_Request)
{
  {
    std::lock_guard<std::mutex> lock(m_CacheQueueMutex);
    m_CacheRequests.push_front(p_Request);
    write(m_CachePipe[1], "1", 1);
  }

  if (m_Connecting || m_OnceConnected)
  {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_Requests.push_front(p_Request);
    write(m_Pipe[1], "1", 1);
    m_RequestsTotal = m_Requests.size();
    m_RequestsDone = 0;
  }
  else
  {
    LOG_DEBUG("async request ignored in offline mode");
  }
}

void ImapManager::PrefetchRequest(const ImapManager::Request& p_Request)
{
  if (m_Connecting || m_OnceConnected)
  {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_PrefetchRequests[p_Request.m_PrefetchLevel].push_front(p_Request);
    write(m_Pipe[1], "1", 1);
    m_PrefetchRequestsTotal = 0;
    for (auto it = m_PrefetchRequests.begin(); it != m_PrefetchRequests.end(); ++it)
    {
      m_PrefetchRequestsTotal += it->second.size();
    }

    m_PrefetchRequestsDone = 0;
  }
  else
  {
    LOG_DEBUG("prefetch request ignored in offline mode");
  }
}

void ImapManager::AsyncAction(const ImapManager::Action& p_Action)
{
  if (m_Connecting || m_OnceConnected)
  {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_Actions.push_front(p_Action);
    write(m_Pipe[1], "1", 1);
  }
  else
  {
    LOG_WARNING("async action not permitted while offline");
  }
}

void ImapManager::AsyncSearch(const SearchQuery& p_SearchQuery)
{
  std::unique_lock<std::mutex> lock(m_SearchMutex);
  m_SearchQueue.push_front(p_SearchQuery);
  m_SearchCond.notify_one();
}

void ImapManager::SetCurrentFolder(const std::string& p_Folder)
{
  m_Mutex.lock();
  m_CurrentFolder = p_Folder;
  m_Mutex.unlock();
}

bool ImapManager::ProcessIdle()
{
  m_Mutex.lock();
  const std::string currentFolder = m_CurrentFolder;
  m_Mutex.unlock();

  bool rv = true;

  if (true)
  {
    // Check mail before enter idle
    Request request;
    request.m_Folder = currentFolder;
    request.m_GetUids = true;
    Response response;
    rv = PerformRequest(request, false /* p_Cached */, false /* p_Prefetch */, response);

    if (rv)
    {
      SendRequestResponse(request, response);
    }
    
    if (!rv)
    {
      return rv;
    }
  }

  int selrv = 0;

  LOG_DEBUG("entering idle");
  while (m_Running && (selrv == 0))
  {
    int idlefd = m_Imap.IdleStart(currentFolder);
    if ((idlefd == -1) || !m_Running)
    {
      rv = false;
      break;
    }

    SetStatus(Status::FlagIdle);

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_Pipe[0], &fds);
    FD_SET(idlefd, &fds);
    int maxfd = std::max(m_Pipe[0], idlefd);
    struct timeval idletv = {(29 * 60), 0};
    selrv = select(maxfd + 1, &fds, NULL, NULL, &idletv);

    m_Imap.IdleDone();
    ClearStatus(Status::FlagIdle);

    if ((selrv != 0) && FD_ISSET(idlefd, &fds) && !m_Running)
    {
      LOG_DEBUG("idle notification");

      int len = 0;
      ioctl(idlefd, FIONREAD, &len);
      if (len > 0)
      {
        std::vector<char> buf(len);
        read(idlefd, &buf[0], len);
      }

      ImapManager::Request request;
      m_Mutex.lock();
      request.m_Folder = m_CurrentFolder;
      m_Mutex.unlock();
      request.m_GetUids = true;
      AsyncRequest(request);
      break;
    }
    else
    {
      LOG_DEBUG("idle timeout/restart");
    }
  }

  LOG_DEBUG("exiting idle");

  return rv;
}

void ImapManager::Process()
{
  THREAD_REGISTER();

  if (m_Connect)
  {
    if (m_Imap.Login())
    {
      SetStatus(Status::FlagConnected);
      m_OnceConnected = true;
    }
    else
    {
      SetStatus(Status::FlagOffline);
      if (m_ResponseHandler)
      {
        ImapManager::Request request;
        Response response;
        response.m_ResponseStatus = ResponseStatusLoginFailed;
        m_ResponseHandler(request, response);
      }
    }

    m_Connecting = false;
    ClearStatus(Status::FlagConnecting);
  }

  LOG_DEBUG("entering loop");
  while (m_Running)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_Pipe[0], &fds);
    int maxfd = m_Pipe[0];
    struct timeval tv = {15, 0};
    int selrv = select(maxfd + 1, &fds, NULL, NULL, &tv);
    bool rv = true;

    if ((selrv == 0) && m_Imap.GetConnected())
    {
      rv &= ProcessIdle();
    }
    else if ((FD_ISSET(m_Pipe[0], &fds)) && m_Imap.GetConnected())
    {
      int len = 0;
      ioctl(m_Pipe[0], FIONREAD, &len);
      if (len > 0)
      {
        std::vector<char> buf(len);
        read(m_Pipe[0], &buf[0], len);
      }

      m_QueueMutex.lock();

      while (m_Running &&
             (!m_Requests.empty() || !m_PrefetchRequests.empty() || !m_Actions.empty()))
      {
        bool isConnected = true;
        
        while (!m_Actions.empty() && m_Running && isConnected)
        {
          Action action = m_Actions.front();
          m_Actions.pop_front();
          m_QueueMutex.unlock();

          bool result = PerformAction(action);
          
          bool retry = false;
          if (!result)
          {
            if (!m_Imap.CheckConnection())
            {
              LOG_WARNING("action failed due to connection lost");
              isConnected = false;
            }
            else if (action.m_TryCount < 2)
            {
              ++action.m_TryCount;
              LOG_WARNING("action retry %d", action.m_TryCount);
              retry = true;
            }
          }

          if (!retry)
          {
            SendActionResult(action, result);
          }

          m_QueueMutex.lock();

          if (retry)
          {
            m_Actions.push_front(action);
          }
        }

        const int progressReportMinTasks = 2;
        while (!m_Requests.empty() && m_Running && isConnected)
        {
          Request request = m_Requests.front();
          m_Requests.pop_front();

          uint32_t progress = (m_RequestsTotal >= progressReportMinTasks) ?
            ((m_RequestsDone * 100) / m_RequestsTotal) : 0;

          m_QueueMutex.unlock();

          SetStatus(Status::FlagFetching, progress);

          Response response;
          bool result = PerformRequest(request, false /* p_Cached */, false /* p_Prefetch */,
                                       response);

          bool retry = false;
          if (!result)
          {
            if (!m_Imap.CheckConnection())
            {
              LOG_WARNING("request failed due to connection lost");
              isConnected = false;
              retry = true;
            }
            else if (request.m_TryCount < 2)
            {
              ++request.m_TryCount;
              LOG_WARNING("request retry %d", request.m_TryCount);
              retry = true;
            }
          }

          if (!retry)
          {
            SendRequestResponse(request, response);
          }

          m_QueueMutex.lock();

          if (retry)
          {
            m_Requests.push_front(request);
          }
          else
          {
            ++m_RequestsDone;
          }
        }

        m_QueueMutex.unlock();
        ClearStatus(Status::FlagFetching);
        m_QueueMutex.lock();

        while (m_Actions.empty() && m_Requests.empty() && !m_PrefetchRequests.empty() &&
               m_Running && isConnected)
        {
          Request request = m_PrefetchRequests.begin()->second.front();
          m_PrefetchRequests.begin()->second.pop_front();
          if (m_PrefetchRequests.begin()->second.empty())
          {
            m_PrefetchRequests.erase(m_PrefetchRequests.begin());
          }

          uint32_t progress = (m_PrefetchRequestsTotal >= progressReportMinTasks) ?
            ((m_PrefetchRequestsDone * 100) / m_PrefetchRequestsTotal) : 0;

          m_QueueMutex.unlock();

          SetStatus(Status::FlagPrefetching, progress);

          Response response;
          bool result = PerformRequest(request, false /* p_Cached */, true /* p_Prefetch */,
                                       response);

          bool retry = false;
          if (!result)
          {
            if (!m_Imap.CheckConnection())
            {
              LOG_WARNING("prefetch request failed due to connection lost");
              isConnected = false;
              retry = true;
            }
            else if (request.m_TryCount < 2)
            {
              ++request.m_TryCount;
              LOG_WARNING("prefetch request retry %d", request.m_TryCount);
              retry = true;
            }
          }

          if (!retry)
          {
            SendRequestResponse(request, response);
          }
          
          m_QueueMutex.lock();

          if (retry)
          {
            m_PrefetchRequests[request.m_PrefetchLevel].push_front(request);
          }
          else
          {
            ++m_PrefetchRequestsDone;
          }
        }

        m_QueueMutex.unlock();
        ClearStatus(Status::FlagPrefetching);

        if (!isConnected)
        {
          LOG_DEBUG("checking connectivity");
          CheckConnectivityAndReconnect();
        }

        m_QueueMutex.lock();
      }

      if (m_Requests.empty())
      {
        m_RequestsTotal = 0;
        m_RequestsDone = 0;
      }

      if (m_PrefetchRequests.empty())
      {
        m_PrefetchRequestsTotal = 0;
        m_PrefetchRequestsDone = 0;
      }

      m_QueueMutex.unlock();
    }

    if (!rv)
    {
      LOG_DEBUG("checking connectivity");
      CheckConnectivityAndReconnect();
    }
  }

  LOG_DEBUG("exiting loop");

  if (m_Connect)
  {
    LOG_DEBUG("logout start");
    m_Imap.Logout();
    LOG_DEBUG("logout complete");
  }

  std::unique_lock<std::mutex> lock(m_ExitedCondMutex);
  m_ExitedCond.notify_one();
}

void ImapManager::CheckConnectivityAndReconnect()
{
  if (!m_Imap.CheckConnection())
  {
    m_Connecting = true;
    SetStatus(Status::FlagConnecting);
    ClearStatus(Status::FlagConnected);
    LOG_WARNING("connection lost");

    m_Imap.Logout();
    bool connected = false;
    while (m_Running)
    {
      LOG_DEBUG("retry connect");
      connected = m_Imap.Login();

      if (connected && m_Running)
      {
        m_Connecting = false;
        SetStatus(Status::FlagConnected);
        ClearStatus(Status::FlagConnecting);
        LOG_INFO("connected");
        break;
      }

      int retryDelay = 15;
      while (m_Running && (retryDelay-- > 0))
      {
        sleep(1);
      }
    }
  }
}

void ImapManager::CacheProcess()
{
  THREAD_REGISTER();

  LOG_DEBUG("entering cache loop");
  while (m_CacheRunning)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_CachePipe[0], &fds);
    int maxfd = m_CachePipe[0];
    struct timeval tv = {60, 0};
    int selrv = select(maxfd + 1, &fds, NULL, NULL, &tv);

    if ((selrv != 0) && FD_ISSET(m_CachePipe[0], &fds))
    {
      int len = 0;
      ioctl(m_CachePipe[0], FIONREAD, &len);
      if (len > 0)
      {
        std::vector<char> buf(len);
        read(m_CachePipe[0], &buf[0], len);
      }

      m_CacheQueueMutex.lock();

      while (m_CacheRunning && !m_CacheRequests.empty())
      {
        while (!m_CacheRequests.empty())
        {
          const Request request = m_CacheRequests.front();
          m_CacheRequests.pop_front();

          m_CacheQueueMutex.unlock();

          Response response;
          bool result = PerformRequest(request, true /* p_Cached */, false /* p_Prefetch */,
                                       response);
          if (!result)
          {
            LOG_WARNING("cache request failed");
          }
          
          SendRequestResponse(request, response);

          m_CacheQueueMutex.lock();
        }
      }

      m_CacheQueueMutex.unlock();
    }
  }

  LOG_DEBUG("exiting cache loop");

  std::unique_lock<std::mutex> lock(m_ExitedCacheCondMutex);
  m_ExitedCacheCond.notify_one();
}

void ImapManager::SearchProcess()
{
  LOG_DEBUG("entering loop");
  while (m_SearchRunning)
  {
    SearchQuery searchQuery;

    {
      std::unique_lock<std::mutex> lock(m_SearchMutex);
      while (m_SearchQueue.empty() && m_SearchRunning)
      {
        m_SearchCond.wait(lock);
      }

      if (!m_SearchRunning)
      {
        break;
      }

      searchQuery = m_SearchQueue.front();
      m_SearchQueue.pop_front();
    }

    PerformSearch(searchQuery);
  }

  LOG_DEBUG("exiting loop");
}

bool ImapManager::PerformRequest(const Request& p_Request, bool p_Cached, bool p_Prefetch,
                                 Response& p_Response)
{
  p_Response.m_ResponseStatus = ResponseStatusOk;
  p_Response.m_Folder = p_Request.m_Folder;
  p_Response.m_Cached = p_Cached;

  if (p_Request.m_GetFolders)
  {
    const bool rv = m_Imap.GetFolders(p_Cached, p_Response.m_Folders);
    p_Response.m_ResponseStatus |= rv ? ResponseStatusOk : ResponseStatusGetFoldersFailed;
  }

  if (p_Request.m_GetUids)
  {
    const bool rv = m_Imap.GetUids(p_Request.m_Folder, p_Cached, p_Response.m_Uids);
    p_Response.m_ResponseStatus |= rv ? ResponseStatusOk : ResponseStatusGetUidsFailed;
  }

  if (!p_Request.m_GetHeaders.empty())
  {
    const bool rv = m_Imap.GetHeaders(p_Request.m_Folder, p_Request.m_GetHeaders, p_Cached,
                                      p_Prefetch, p_Response.m_Headers);
    p_Response.m_ResponseStatus |= rv ? ResponseStatusOk : ResponseStatusGetHeadersFailed;
  }

  if (!p_Request.m_GetFlags.empty())
  {
    const bool rv = m_Imap.GetFlags(p_Request.m_Folder, p_Request.m_GetFlags, p_Cached,
                                    p_Response.m_Flags);
    p_Response.m_ResponseStatus |= rv ? ResponseStatusOk : ResponseStatusGetFlagsFailed;
  }

  if (!p_Request.m_GetBodys.empty())
  {
    const bool rv = m_Imap.GetBodys(p_Request.m_Folder, p_Request.m_GetBodys, p_Cached,
                                    p_Prefetch, p_Response.m_Bodys);
    p_Response.m_ResponseStatus |= rv ? ResponseStatusOk : ResponseStatusGetBodysFailed;
  }

  return (p_Response.m_ResponseStatus == ResponseStatusOk);
}

bool ImapManager::PerformAction(const ImapManager::Action& p_Action)
{
  bool rv = true;

  if (!p_Action.m_MoveDestination.empty())
  {
    SetStatus(Status::FlagMoving);
    rv &= m_Imap.MoveMessages(p_Action.m_Folder, p_Action.m_Uids, p_Action.m_MoveDestination);
    ClearStatus(Status::FlagMoving);
  }

  if (p_Action.m_SetSeen || p_Action.m_SetUnseen)
  {
    SetStatus(Status::FlagUpdatingFlags);
    rv &= m_Imap.SetFlagSeen(p_Action.m_Folder, p_Action.m_Uids, p_Action.m_SetSeen);
    ClearStatus(Status::FlagUpdatingFlags);
  }

  if (p_Action.m_UploadDraft)
  {
    SetStatus(Status::FlagSaving);
    rv &= m_Imap.UploadMessage(p_Action.m_Folder, p_Action.m_Msg, true);
    ClearStatus(Status::FlagSaving);
  }

  if (p_Action.m_UploadMessage)
  {
    SetStatus(Status::FlagSaving);
    rv &= m_Imap.UploadMessage(p_Action.m_Folder, p_Action.m_Msg, false);
    ClearStatus(Status::FlagSaving);
  }

  if (p_Action.m_DeleteMessages)
  {
    SetStatus(Status::FlagDeleting);
    rv &= m_Imap.DeleteMessages(p_Action.m_Folder, p_Action.m_Uids);
    ClearStatus(Status::FlagDeleting);
  }

  return rv;
}

void ImapManager::PerformSearch(const SearchQuery& p_SearchQuery)
{
  SearchResult searchResult;
  m_Imap.Search(p_SearchQuery.m_QueryStr, p_SearchQuery.m_Offset, p_SearchQuery.m_Max,
                searchResult.m_Headers, searchResult.m_FolderUids, searchResult.m_HasMore);

  if (m_SearchHandler)
  {
    m_SearchHandler(p_SearchQuery, searchResult);
  }
}

void ImapManager::SendRequestResponse(const Request& p_Request, const Response& p_Response)
{
  if (m_ResponseHandler)
  {
    m_ResponseHandler(p_Request, p_Response);
  }
}

void ImapManager::SendActionResult(const Action& p_Action, bool p_Result)
{
  Result result;
  result.m_Result = p_Result;

  if (m_ResultHandler)
  {
    m_ResultHandler(p_Action, result);
  }
}

void ImapManager::SetStatus(uint32_t p_Flags, int32_t p_Progress /* = -1 */)
{
  StatusUpdate statusUpdate;
  statusUpdate.SetFlags = p_Flags;
  statusUpdate.Progress = p_Progress;
  if (m_StatusHandler)
  {
    m_StatusHandler(statusUpdate);
  }
}

void ImapManager::ClearStatus(uint32_t p_Flags)
{
  StatusUpdate statusUpdate;
  statusUpdate.ClearFlags = p_Flags;
  if (m_StatusHandler)
  {
    m_StatusHandler(statusUpdate);
  }
}
