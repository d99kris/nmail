// imapmanager.cpp
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "imapmanager.h"

#include <vector>

#include "loghelp.h"

ImapManager::ImapManager(const std::string& p_User, const std::string& p_Pass,
                         const std::string& p_Host, const uint16_t p_Port,
                         const bool p_Connect, const bool p_CacheEncrypt,
                         const std::function<void(const ImapManager::Request&,const ImapManager::Response&)>& p_ResponseHandler,
                         const std::function<void(const ImapManager::Action&,const ImapManager::Result&)>& p_ResultHandler,
                         const std::function<void(const StatusUpdate&)>& p_StatusHandler)
  : m_Imap(p_User, p_Pass, p_Host, p_Port, p_CacheEncrypt)
  , m_Connect(p_Connect)
  , m_ResponseHandler(p_ResponseHandler)
  , m_ResultHandler(p_ResultHandler)
  , m_StatusHandler(p_StatusHandler)
  , m_Connecting(false)
  , m_Running(false)
  , m_CacheRunning(false)
{
  pipe(m_Pipe);
  pipe(m_CachePipe);
  m_Connecting = m_Connect;
  SetStatus(m_Connecting ? Status::FlagConnecting : Status::FlagOffline);
  m_Running = true;
  m_CacheRunning = true;
  LOG_DEBUG("start threads");
  m_Thread = std::thread(&ImapManager::Process, this);
  m_CacheThread = std::thread(&ImapManager::CacheProcess, this);
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
  
  close(m_Pipe[0]);
  close(m_Pipe[1]);
  close(m_CachePipe[0]);
  close(m_CachePipe[1]);
}

void ImapManager::AsyncRequest(const ImapManager::Request &p_Request)
{
  {
    std::lock_guard<std::mutex> lock(m_CacheQueueMutex);
    m_CacheRequests.push_front(p_Request);
    write(m_CachePipe[1], "1", 1);
  }

  if (m_Imap.GetConnected() || m_Connecting)
  {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_Requests.push_front(p_Request);
    write(m_Pipe[1], "1", 1);
    m_RequestsTotal = m_Requests.size();
    m_RequestsDone = 0;
  }
}

void ImapManager::PrefetchRequest(const ImapManager::Request &p_Request)
{
  {
    std::lock_guard<std::mutex> lock(m_CacheQueueMutex);
    m_CacheRequests.push_front(p_Request);
    write(m_CachePipe[1], "1", 1);
  }

  if (m_Imap.GetConnected() || m_Connecting)
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
}

void ImapManager::AsyncAction(const ImapManager::Action &p_Action)
{
  if (m_Imap.GetConnected())
  {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_Actions.push_front(p_Action);
    write(m_Pipe[1], "1", 1);
  }
  else
  {
    LOG_WARNING("action not permitted while offline");
  }
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
    ImapManager::Request request;
    request.m_Folder = currentFolder;
    request.m_GetUids = true;
    rv = PerformRequest(request, false);

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
    if (idlefd == -1)
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
    
    if ((selrv != 0) && FD_ISSET(idlefd, &fds))
    {
      LOG_DEBUG("idle notification");

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
  if (m_Connect)
  {
    if (m_Imap.Login())
    {
      m_Connecting = false;
      SetStatus(Status::FlagConnected);
      ClearStatus(Status::FlagConnecting);
    }
  }

  LOG_DEBUG("entering loop");
  while (m_Running)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_Pipe[0], &fds);
    int maxfd = m_Pipe[0];
    struct timeval tv = {60, 0};
    int selrv = select(maxfd + 1, &fds, NULL, NULL, &tv);
    bool rv = true;

    if ((selrv == 0) && m_Imap.GetConnected())
    {
      rv &= ProcessIdle();
    }
    else if (FD_ISSET(m_Pipe[0], &fds))
    {
      int len = 0;
      ioctl(m_Pipe[0], FIONREAD, &len);
      if (len > 0)
      {
        len = std::min(len, 256);
        std::vector<char> buf(len);
        read(m_Pipe[0], &buf[0], len);
      }

      m_QueueMutex.lock();

      while (m_Running &&
             (!m_Requests.empty() || !m_PrefetchRequests.empty() || !m_Actions.empty()))
      {
        while (!m_Actions.empty())
        {
          const Action action = m_Actions.front();
          m_Actions.pop_front();
          m_QueueMutex.unlock();

          rv &= PerformAction(action);

          m_QueueMutex.lock();
        }

        const int progressReportMinTasks = 2;
        while (!m_Requests.empty())
        {
          const Request request = m_Requests.front();
          m_Requests.pop_front();

          uint32_t progress = (m_RequestsTotal >= progressReportMinTasks) ?
            ((m_RequestsDone * 100) / m_RequestsTotal) : 0;

          m_QueueMutex.unlock();

          SetStatus(Status::FlagFetching, progress);

          rv &= PerformRequest(request, false);

          m_QueueMutex.lock();

          ++m_RequestsDone;
        }

        m_QueueMutex.unlock();
        ClearStatus(Status::FlagFetching);        
        m_QueueMutex.lock();

        if (!m_PrefetchRequests.empty())
        {
          const Request request = m_PrefetchRequests.begin()->second.front();
          m_PrefetchRequests.begin()->second.pop_front();
          if (m_PrefetchRequests.begin()->second.empty())
          {
            m_PrefetchRequests.erase(m_PrefetchRequests.begin());
          }

          uint32_t progress = (m_PrefetchRequestsTotal >= progressReportMinTasks) ?
            ((m_PrefetchRequestsDone * 100) / m_PrefetchRequestsTotal) : 0;

          m_QueueMutex.unlock();

          SetStatus(Status::FlagPrefetching, progress);

          rv &= PerformRequest(request, false);

          m_QueueMutex.lock();

          ++m_PrefetchRequestsDone;
        }

        m_QueueMutex.unlock();
        ClearStatus(Status::FlagPrefetching);
        m_QueueMutex.lock();
      }

      m_RequestsTotal = 0;
      m_RequestsDone = 0;
      m_PrefetchRequestsTotal = 0;
      m_PrefetchRequestsDone = 0;

      m_QueueMutex.unlock();
    }

    if (!rv)
    {
      LOG_DEBUG("checking connectivity");

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

void ImapManager::CacheProcess()
{
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
        len = std::min(len, 256);
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

          PerformRequest(request, true);

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

bool ImapManager::PerformRequest(const ImapManager::Request& p_Request, bool p_Cached)
{
  Response response;

  response.m_ResponseStatus = ResponseStatusOk;
  response.m_Folder = p_Request.m_Folder;
  response.m_Cached = p_Cached;
  if (p_Request.m_GetFolders)
  {
    const bool rv = m_Imap.GetFolders(p_Cached, response.m_Folders);
    response.m_ResponseStatus |= rv ? ResponseStatusOk : ResponseStatusGetFoldersFailed;
  }

  if (p_Request.m_GetUids)
  {
    const bool rv = m_Imap.GetUids(p_Request.m_Folder, p_Cached, response.m_Uids);
    response.m_ResponseStatus |= rv ? ResponseStatusOk : ResponseStatusGetUidsFailed;
  }

  if (!p_Request.m_GetHeaders.empty())
  {
    const bool rv = m_Imap.GetHeaders(p_Request.m_Folder, p_Request.m_GetHeaders, p_Cached, response.m_Headers);
    response.m_ResponseStatus |= rv ? ResponseStatusOk : ResponseStatusGetHeadersFailed;
  }

  if (!p_Request.m_GetFlags.empty())
  {
    const bool rv = m_Imap.GetFlags(p_Request.m_Folder, p_Request.m_GetFlags, p_Cached, response.m_Flags);
    response.m_ResponseStatus |= rv ? ResponseStatusOk : ResponseStatusGetFlagsFailed;
  }

  if (!p_Request.m_GetBodys.empty())
  {
    const bool rv = m_Imap.GetBodys(p_Request.m_Folder, p_Request.m_GetBodys, p_Cached, response.m_Bodys);
    response.m_ResponseStatus |= rv ? ResponseStatusOk : ResponseStatusGetBodysFailed;
  }

  if (m_ResponseHandler)
  {
    m_ResponseHandler(p_Request, response);
  }

  return (response.m_ResponseStatus == ResponseStatusOk);
}

bool ImapManager::PerformAction(const ImapManager::Action& p_Action)
{
  Result result;
  result.m_Result = true;

  if (!p_Action.m_MoveDestination.empty())
  {
    SetStatus(Status::FlagMoving);
    result.m_Result &= m_Imap.MoveMessages(p_Action.m_Folder, p_Action.m_Uids,
                                           p_Action.m_MoveDestination);
    ClearStatus(Status::FlagMoving);
  }

  if (p_Action.m_SetSeen || p_Action.m_SetUnseen)
  {
    SetStatus(Status::FlagUpdatingFlags);
    result.m_Result &= m_Imap.SetFlagSeen(p_Action.m_Folder, p_Action.m_Uids, p_Action.m_SetSeen);
    ClearStatus(Status::FlagUpdatingFlags);
  }

  if (p_Action.m_UploadDraft)
  {
    SetStatus(Status::FlagSaving);
    result.m_Result &= m_Imap.UploadMessage(p_Action.m_Folder, p_Action.m_Msg, true);
    ClearStatus(Status::FlagSaving);
  }

  if (m_ResultHandler)
  {
    m_ResultHandler(p_Action, result);
  }

  return (result.m_Result);
}

void ImapManager::SetStatus(uint32_t p_Flags, uint32_t p_Progress /* = 0 */)
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
