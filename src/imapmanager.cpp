// imapmanager.cpp
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "imapmanager.h"

#include <vector>

#include "auth.h"
#include "loghelp.h"
#include "util.h"

ImapManager::ImapManager(const std::string& p_User, const std::string& p_Pass,
                         const std::string& p_Host, const uint16_t p_Port,
                         const bool p_Connect, const int64_t p_Timeout,
                         const bool p_CacheEncrypt,
                         const bool p_CacheIndexEncrypt,
                         const bool p_IdleFetchFlags,
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
  , m_IdleFetchFlags(p_IdleFetchFlags)
  , m_Connecting(false)
  , m_Running(false)
  , m_CacheRunning(false)
  , m_Aborting(false)
{
  LOG_IF_NONZERO(pipe(m_Pipe));
  LOG_IF_NONZERO(pipe(m_CachePipe));
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
    LOG_IF_NOT_EQUAL(write(m_Pipe[1], "1", 1), 1);

    if (m_ExitedCond.wait_for(lock, std::chrono::seconds(3)) != std::cv_status::timeout)
    {
      m_Thread.join();
      LOG_DEBUG("process thread joined");
    }
    else
    {
      LOG_WARNING("process thread exit timeout");

      LOG_DEBUG("process thread abort");
      m_Aborting = true;
      m_Imap.SetAborting(true);
      pthread_kill(m_ThreadId, SIGUSR2);
      if (m_ExitedCond.wait_for(lock, std::chrono::seconds(1)) != std::cv_status::timeout)
      {
        m_Thread.join();
        LOG_DEBUG("process thread joined");
      }
      else
      {
        LOG_WARNING("process thread abort timeout");
        lock.unlock();
        m_Thread.join();
      }
    }
  }

  {
    std::unique_lock<std::mutex> lock(m_ExitedCacheCondMutex);

    m_CacheRunning = false;
    LOG_IF_NOT_EQUAL(write(m_CachePipe[1], "1", 1), 1);

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
    LOG_IF_NOT_EQUAL(write(m_CachePipe[1], "1", 1), 1);
  }

  if (m_Connecting || m_OnceConnected)
  {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_Requests.push_front(p_Request);
    LOG_IF_NOT_EQUAL(write(m_Pipe[1], "1", 1), 1);
    ProgressCountRequestAdd(p_Request, false /* p_IsPrefetch */);
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
    LOG_IF_NOT_EQUAL(write(m_Pipe[1], "1", 1), 1);
    ProgressCountRequestAdd(p_Request, true /* p_IsPrefetch */);
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
    LOG_IF_NOT_EQUAL(write(m_Pipe[1], "1", 1), 1);
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
  static bool firstIdle = true;

  if (true)
  {
    // Get folders if not done before
    if (firstIdle)
    {
      firstIdle = false;

      // Check cache
      Request request;
      request.m_GetFolders = true;
      Response response;
      PerformRequest(request, true /* p_Cached */, false /* p_Prefetch */, response);
      if (response.m_Folders.empty())
      {
        // Fetch folders if cached list is empty
        PerformRequest(request, false /* p_Cached */, false /* p_Prefetch */, response);
      }
    }

    // Check mail before enter idle
    Request uidsRequest;
    uidsRequest.m_Folder = currentFolder;
    uidsRequest.m_GetUids = true;
    Response uidsResponse;
    rv = PerformRequest(uidsRequest, false /* p_Cached */, false /* p_Prefetch */, uidsResponse);
    if (rv)
    {
      SendRequestResponse(uidsRequest, uidsResponse);
    }
    else
    {
      return rv;
    }

    // Check flags before enter idle
    if (m_IdleFetchFlags)
    {
      Request flagsRequest;
      flagsRequest.m_Folder = currentFolder;
      flagsRequest.m_GetFlags = uidsResponse.m_Uids;
      Response flagsResponse;
      rv = PerformRequest(flagsRequest, false /* p_Cached */, false /* p_Prefetch */, flagsResponse);
      if (rv)
      {
        SendRequestResponse(flagsRequest, flagsResponse);
      }
      else
      {
        return rv;
      }
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
    struct timeval idletv = {GetIdleDurationSec(), 0};
    selrv = select(maxfd + 1, &fds, NULL, NULL, &idletv);

    bool idleRv = m_Imap.IdleDone();
    ClearStatus(Status::FlagIdle);

    if (!idleRv)
    {
      rv = false;
      break;
    }

    if ((selrv != 0) && FD_ISSET(idlefd, &fds) && m_Running)
    {
      LOG_DEBUG("idle notification");

      int len = 0;
      ioctl(idlefd, FIONREAD, &len);
      if (len > 0)
      {
        std::vector<char> buf(len);
        LOG_IF_NOT_EQUAL(read(idlefd, &buf[0], len), len);
      }

      ImapManager::Request request;
      m_Mutex.lock();
      request.m_Folder = m_CurrentFolder;
      m_Mutex.unlock();
      request.m_GetUids = true;
      request.m_IdleWake = m_IdleFetchFlags;
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

int ImapManager::GetIdleDurationSec()
{
  int idleDuration = (29 * 60);
  if (Auth::IsOAuthEnabled())
  {
    int timeToOAuthExpiry = static_cast<int>(Auth::GetTimeToExpirySec());
    if ((timeToOAuthExpiry < idleDuration) && (timeToOAuthExpiry > 0))
    {
      idleDuration = timeToOAuthExpiry;
      LOG_DEBUG("idle duration from oauth2 expiry %d", idleDuration);
    }
  }

  return idleDuration;
}

void ImapManager::ProcessIdleOffline()
{
  LOG_DEBUG("entering idle");
  m_Imap.IndexNotifyIdle(true);

  int selrv = 0;
  int idleDuration = (29 * 60);
  while (m_Running && (selrv == 0))
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_Pipe[0], &fds);
    int maxfd = m_Pipe[0];
    struct timeval idletv = {idleDuration, 0};
    selrv = select(maxfd + 1, &fds, NULL, NULL, &idletv);
  }

  m_Imap.IndexNotifyIdle(false);
  LOG_DEBUG("exiting idle");
}

void ImapManager::Process()
{
  THREAD_REGISTER();
  m_ThreadId = pthread_self();

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

    int selrv = 1;
    m_QueueMutex.lock();
    bool isQueueEmpty = m_Requests.empty() && m_PrefetchRequests.empty() && m_Actions.empty();
    m_QueueMutex.unlock();

    if (isQueueEmpty)
    {
      selrv = select(maxfd + 1, &fds, NULL, NULL, &tv);
    }

    bool idleRv = true;
    bool authRefreshNeeded = AuthRefreshNeeded();

    if ((selrv == 0) && !authRefreshNeeded && isQueueEmpty)
    {
      if (m_Imap.GetConnected())
      {
        idleRv &= ProcessIdle();
      }
      else
      {
        ProcessIdleOffline();
      }
    }
    else if ((FD_ISSET(m_Pipe[0], &fds) || !isQueueEmpty) && m_Imap.GetConnected() &&
             !authRefreshNeeded)
    {
      int len = 0;
      ioctl(m_Pipe[0], FIONREAD, &len);
      if (len > 0)
      {
        std::vector<char> buf(len);
        LOG_IF_NOT_EQUAL(read(m_Pipe[0], &buf[0], len), len);
      }

      m_QueueMutex.lock();

      while (m_Running && !authRefreshNeeded &&
             (!m_Requests.empty() || !m_PrefetchRequests.empty() || !m_Actions.empty()))
      {
        bool isConnected = true;
        float progress = 0;

        while (!m_Actions.empty() && m_Running && isConnected && !authRefreshNeeded)
        {
          Action action = m_Actions.front();
          m_Actions.pop_front();
          m_QueueMutex.unlock();

          bool result = PerformAction(action);

          bool retry = false;
          if (!result)
          {
            if (!CheckConnectivity())
            {
              LOG_WARNING("action failed due to connection lost");
              SetStatus(Status::FlagConnecting);
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

          authRefreshNeeded = AuthRefreshNeeded();

          m_QueueMutex.lock();

          if (retry)
          {
            m_Actions.push_front(action);
          }
        }

        progress = 0;
        while (!m_Requests.empty() && m_Running && isConnected && !authRefreshNeeded)
        {
          Request request = m_Requests.front();
          m_Requests.pop_front();

          m_QueueMutex.unlock();

          SetStatus(Status::FlagFetching, progress);

          Response response;
          bool result = PerformRequest(request, false /* p_Cached */, false /* p_Prefetch */,
                                       response);

          bool retry = false;
          if (!result)
          {
            if (!CheckConnectivity())
            {
              LOG_WARNING("request failed due to connection lost");
              SetStatus(Status::FlagConnecting);
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

          authRefreshNeeded = AuthRefreshNeeded();

          m_QueueMutex.lock();

          if (retry)
          {
            m_Requests.push_front(request);
          }
          else
          {
            ProgressCountRequestDone(request, false /* p_IsPrefetch */);
            progress = GetProgressPercentage(request, false /* p_IsPrefetch */);
          }
        }

        m_QueueMutex.unlock();
        ClearStatus(Status::FlagFetching);
        m_QueueMutex.lock();

        progress = 0;
        while (m_Actions.empty() && m_Requests.empty() && !m_PrefetchRequests.empty() &&
               m_Running && isConnected && !authRefreshNeeded)
        {
          Request request = m_PrefetchRequests.begin()->second.front();
          m_PrefetchRequests.begin()->second.pop_front();
          if (m_PrefetchRequests.begin()->second.empty())
          {
            m_PrefetchRequests.erase(m_PrefetchRequests.begin());
          }

          m_QueueMutex.unlock();

          SetStatus(Status::FlagPrefetching, progress);

          Response response;
          bool result = PerformRequest(request, false /* p_Cached */, true /* p_Prefetch */,
                                       response);

          bool retry = false;
          if (!result)
          {
            if (!CheckConnectivity())
            {
              LOG_WARNING("prefetch request failed due to connection lost");
              SetStatus(Status::FlagConnecting);
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

          authRefreshNeeded = AuthRefreshNeeded();

          m_QueueMutex.lock();

          if (retry)
          {
            m_PrefetchRequests[request.m_PrefetchLevel].push_front(request);
          }
          else
          {
            ProgressCountRequestDone(request, true /* p_IsPrefetch */);
            progress = GetProgressPercentage(request, true /* p_IsPrefetch */);
          }
        }

        m_QueueMutex.unlock();
        ClearStatus(Status::FlagPrefetching);

        if (!isConnected)
        {
          LOG_WARNING("processing failed");
          CheckConnectivityAndReconnect(!isConnected);
        }

        m_QueueMutex.lock();
      }

      if (m_Requests.empty())
      {
        ProgressCountReset(false /* p_IsPrefetch */);
      }

      if (m_PrefetchRequests.empty())
      {
        ProgressCountReset(true /* p_IsPrefetch */);
      }

      isQueueEmpty = m_Requests.empty() && m_PrefetchRequests.empty() && m_Actions.empty();

      m_QueueMutex.unlock();
    }

    if (m_Running && !idleRv && !authRefreshNeeded)
    {
      LOG_WARNING("idle failed");
      CheckConnectivityAndReconnect(false);
    }

    if (authRefreshNeeded)
    {
      PerformAuthRefresh();
    }
  }

  LOG_DEBUG("exiting loop");

  if (m_Aborting)
  {
    LOG_DEBUG("skip logout");
  }
  else
  {
    if (m_Connect)
    {
      LOG_DEBUG("logout start");
      m_Imap.Logout();
      LOG_DEBUG("logout complete");
    }
  }

  std::unique_lock<std::mutex> lock(m_ExitedCondMutex);
  m_ExitedCond.notify_one();
}

bool ImapManager::AuthRefreshNeeded()
{
  return m_Connect && Auth::IsOAuthEnabled() && Auth::RefreshNeeded();
}

bool ImapManager::PerformAuthRefresh()
{
  return m_Imap.AuthRefresh();
}

bool ImapManager::CheckConnectivity()
{
  SetStatus(Status::FlagChecking);
  bool rv = m_Imap.CheckConnection();
  ClearStatus(Status::FlagChecking);
  return rv;
}

void ImapManager::CheckConnectivityAndReconnect(bool p_SkipCheck)
{
  if (p_SkipCheck || !CheckConnectivity())
  {
    LOG_WARNING("connection lost");

    m_Connecting = true;
    SetStatus(Status::FlagConnecting);
    ClearStatus(Status::FlagConnected);

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
        LOG_IF_NOT_EQUAL(read(m_CachePipe[0], &buf[0], len), len);
      }

      m_CacheQueueMutex.lock();

      while (m_CacheRunning && !m_CacheRequests.empty())
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
    if (p_Request.m_ProcessHtml)
    {
      for (auto& body : p_Response.m_Bodys)
      {
        // pre-convert html to text to improve ui latency
        body.second.GetTextHtml();
      }
    }

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

  if (p_Action.m_UpdateCache && !p_Action.m_SetBodysCache.empty())
  {
    rv &= m_Imap.SetBodysCache(p_Action.m_Folder, p_Action.m_SetBodysCache);
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

void ImapManager::SetStatus(uint32_t p_Flags, float p_Progress /* = -1 */)
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

void ImapManager::ProgressCountRequestAdd(const Request& p_Request, bool p_IsPrefetch)
{
  ProgressCount& progressCount = p_IsPrefetch ? m_PrefetchProgressCount : m_FetchProgressCount;
  if (p_Request.m_GetUids)
  {
    ++progressCount.m_ListTotal;
  }
  else if (!p_Request.m_Folder.empty())
  {
    ++progressCount.m_ItemTotal[p_Request.m_Folder];
    ++progressCount.m_ItemTotal[""];
  }
}

void ImapManager::ProgressCountRequestDone(const Request& p_Request, bool p_IsPrefetch)
{
  ProgressCount& progressCount = p_IsPrefetch ? m_PrefetchProgressCount : m_FetchProgressCount;
  if (p_Request.m_GetUids)
  {
    ++progressCount.m_ListDone;
  }
  else if (!p_Request.m_Folder.empty())
  {
    ++progressCount.m_ItemDone[p_Request.m_Folder];
    ++progressCount.m_ItemDone[""];
  }
}

void ImapManager::ProgressCountReset(bool p_IsPrefetch)
{
  ProgressCount& progressCount = p_IsPrefetch ? m_PrefetchProgressCount : m_FetchProgressCount;
  progressCount.m_ListTotal = 0;
  progressCount.m_ListDone = 0;
  progressCount.m_ItemTotal.clear();
  progressCount.m_ItemDone.clear();
}

float ImapManager::GetProgressPercentage(const Request& p_Request, bool p_IsPrefetch)
{
  ProgressCount& progressCount = p_IsPrefetch ? m_PrefetchProgressCount : m_FetchProgressCount;
  float progress = 0;
  static const float factor = 100.0;
  if (progressCount.m_ListTotal > 0)
  {
    const float listPart = (factor * std::max(0.0f, (float)(progressCount.m_ListDone - 1))) / (float)progressCount.m_ListTotal;
    float itemPart = 0;
    const std::string& folder = p_Request.m_Folder;
    if (!folder.empty() && (progressCount.m_ItemTotal[folder] > 0))
    {
      itemPart = (factor * (float)progressCount.m_ItemDone[folder]) / (float)progressCount.m_ItemTotal[folder];
    }

    progress = listPart + (itemPart / (float)progressCount.m_ListTotal);
  }
  else
  {
    if (progressCount.m_ItemTotal[""] > 0)
    {
      progress = (factor * (float)progressCount.m_ItemDone[""]) / (float)progressCount.m_ItemTotal[""];
    }
    else
    {
      progress = 0;
    }
  }

  return progress;
}
