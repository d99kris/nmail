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
{
  pipe(m_Pipe);
  m_Connecting = m_Connect;
  SetStatus(m_Connecting ? Status::FlagConnecting : Status::FlagOffline);
  m_Running = true;
  LOG_DEBUG("start thread");
  m_Thread = std::thread(&ImapManager::Process, this);
}

ImapManager::~ImapManager()
{
  LOG_DEBUG("stop thread");
  std::unique_lock<std::mutex> lock(m_ExitedCondMutex);

  if (m_QueueMutex.try_lock())
  {
    m_Requests.clear();
    m_PrefetchRequests.clear();
    m_Actions.clear();
    m_QueueMutex.unlock();
  }
  
  m_Running = false;
  write(m_Pipe[1], "1", 1);

  if (m_ExitedCond.wait_for(lock, std::chrono::seconds(5)) != std::cv_status::timeout)
  {
    m_Thread.join();
    LOG_DEBUG("thread joined");
  }
  else
  {
    LOG_WARNING("thread exit timeout");
  }

  close(m_Pipe[0]);
  close(m_Pipe[1]);
}

void ImapManager::AsyncRequest(const ImapManager::Request &p_Request)
{
  PerformRequest(p_Request, true);
  if (m_Imap.GetConnected() || m_Connecting)
  {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_Requests.push_front(p_Request);
    write(m_Pipe[1], "1", 1);
  }
}

void ImapManager::PrefetchRequest(const ImapManager::Request &p_Request)
{
  std::lock_guard<std::mutex> lock(m_QueueMutex);
  m_PrefetchRequests[p_Request.m_PrefetchLevel].push_front(p_Request);
  write(m_Pipe[1], "1", 1);
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

void ImapManager::ProcessIdle()
{
  m_Mutex.lock();
  const std::string currentFolder = m_CurrentFolder;
  m_Mutex.unlock();

  if (true)
  {
    // Check mail before enter idle
    ImapManager::Request request;
    request.m_Folder = currentFolder;
    request.m_GetUids = true;
    PerformRequest(request, false);
  }

  int rv = 0;

  LOG_DEBUG("entering idle");
  while (m_Running && (rv == 0))
  {
    int idlefd = m_Imap.IdleStart(currentFolder);
    if (idlefd == -1) break;

    SetStatus(Status::FlagIdle);

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_Pipe[0], &fds);
    FD_SET(idlefd, &fds);
    int maxfd = std::max(m_Pipe[0], idlefd);
    struct timeval idletv = {(29 * 60), 0};
    rv = select(maxfd + 1, &fds, NULL, NULL, &idletv);

    m_Imap.IdleDone();
    ClearStatus(Status::FlagIdle);
    
    if ((rv != 0) && FD_ISSET(idlefd, &fds))
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
}

void ImapManager::Process()
{
  if (m_Connect)
  {
    bool connected = m_Imap.Login();
    m_Connecting = false;
    ClearStatus(Status::FlagConnecting);
    SetStatus(connected ? Status::FlagConnected : Status::FlagOffline);
  }

  LOG_DEBUG("entering loop");
  while (m_Running)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_Pipe[0], &fds);
    int maxfd = m_Pipe[0];
    struct timeval tv = {60, 0};
    int rv = select(maxfd + 1, &fds, NULL, NULL, &tv);

    if ((rv == 0) && m_Imap.GetConnected())
    {
      ProcessIdle();
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

          PerformAction(action);

          m_QueueMutex.lock();
        }

        while (!m_Requests.empty())
        {
          const Request request = m_Requests.front();
          m_Requests.pop_front();
          m_QueueMutex.unlock();

          SetStatus(Status::FlagFetching);

          PerformRequest(request, false);

          ClearStatus(Status::FlagFetching);

          m_QueueMutex.lock();
        }

        if (!m_PrefetchRequests.empty())
        {
          const Request request = m_PrefetchRequests.begin()->second.front();
          m_PrefetchRequests.begin()->second.pop_front();
          if (m_PrefetchRequests.begin()->second.empty())
          {
            m_PrefetchRequests.erase(m_PrefetchRequests.begin());
          }

          m_QueueMutex.unlock();

          SetStatus(Status::FlagPrefetching);

          PerformRequest(request, false);

          ClearStatus(Status::FlagPrefetching);

          m_QueueMutex.lock();
        }
      }

      m_QueueMutex.unlock();
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

void ImapManager::PerformRequest(const ImapManager::Request& p_Request, bool p_Cached)
{
  Response response;
  
  response.m_Folder = p_Request.m_Folder;
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
}

void ImapManager::PerformAction(const ImapManager::Action& p_Action)
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
    result.m_Result = m_Imap.SetFlagSeen(p_Action.m_Folder, p_Action.m_Uids, p_Action.m_SetSeen);
    ClearStatus(Status::FlagUpdatingFlags);
  }

  if (m_ResultHandler)
  {
    m_ResultHandler(p_Action, result);
  }
}

void ImapManager::SetStatus(uint32_t p_Flags)
{
  StatusUpdate statusUpdate;
  statusUpdate.SetFlags = p_Flags;
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
