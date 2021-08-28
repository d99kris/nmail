// smtpmanager.cpp
//
// Copyright (c) 2019-2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "smtpmanager.h"

#include <sys/ioctl.h>

#include "loghelp.h"
#include "smtp.h"

SmtpManager::SmtpManager(const std::string& p_User, const std::string& p_Pass,
                         const std::string& p_Host, const uint16_t p_Port,
                         const std::string& p_Name, const std::string& p_Address,
                         const bool p_Connect, const int64_t p_Timeout,
                         const std::function<void(const SmtpManager::Result&)>& p_ResultHandler,
                         const std::function<void(const StatusUpdate&)>& p_StatusHandler)
  : m_User(p_User)
  , m_Pass(p_Pass)
  , m_Host(p_Host)
  , m_Port(p_Port)
  , m_Name(p_Name)
  , m_Address(p_Address)
  , m_Connect(p_Connect)
  , m_Timeout(p_Timeout)
  , m_ResultHandler(p_ResultHandler)
  , m_StatusHandler(p_StatusHandler)
  , m_Running(false)
{
  LOG_IF_NONZERO(pipe(m_Pipe));
}

SmtpManager::~SmtpManager()
{
  LOG_DEBUG("stop thread");
  std::unique_lock<std::mutex> lock(m_ExitedCondMutex);

  m_Running = false;
  LOG_IF_NOT_EQUAL(write(m_Pipe[1], "1", 1), 1);

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

void SmtpManager::Start()
{
  m_Running = true;
  LOG_DEBUG("start thread");
  m_Thread = std::thread(&SmtpManager::Process, this);
}

void SmtpManager::AsyncAction(const SmtpManager::Action& p_Action)
{
  if (m_Connect || p_Action.m_IsCreateMessage)
  {
    m_Actions.push_front(p_Action);
    LOG_IF_NOT_EQUAL(write(m_Pipe[1], "1", 1), 1);
  }
  else
  {
    LOG_WARNING("action not permitted while offline");
  }
}

SmtpManager::Result SmtpManager::SyncAction(const SmtpManager::Action& p_Action)
{
  if (m_Connect || p_Action.m_IsCreateMessage)
  {
    return PerformAction(p_Action);
  }
  else
  {
    LOG_WARNING("action not permitted while offline");
    Result result;
    result.m_Result = false;
    result.m_Action = p_Action;
    return result;
  }
}

std::string SmtpManager::GetAddress()
{
  return m_Address;
}

void SmtpManager::Process()
{
  THREAD_REGISTER();

  LOG_DEBUG("entering loop");
  while (m_Running)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_Pipe[0], &fds);
    int maxfd = m_Pipe[0];
    struct timeval tv = {60, 0};
    int rv = select(maxfd + 1, &fds, NULL, NULL, &tv);

    if (rv == 0) continue;

    if (FD_ISSET(m_Pipe[0], &fds))
    {
      int len = 0;
      ioctl(m_Pipe[0], FIONREAD, &len);
      if (len > 0)
      {
        len = std::min(len, 256);
        std::vector<char> buf(len);
        LOG_IF_NOT_EQUAL(read(m_Pipe[0], &buf[0], len), len);
      }

      m_QueueMutex.lock();

      while (m_Running && (!m_Actions.empty()))
      {
        while (!m_Actions.empty())
        {
          const Action action = m_Actions.front();
          m_Actions.pop_front();
          m_QueueMutex.unlock();

          const Result& result = PerformAction(action);

          if (m_ResultHandler)
          {
            m_ResultHandler(result);
          }

          m_QueueMutex.lock();
        }
      }

      m_QueueMutex.unlock();
    }
  }

  LOG_DEBUG("exiting loop");

  std::unique_lock<std::mutex> lock(m_ExitedCondMutex);
  m_ExitedCond.notify_one();
}

SmtpManager::Result SmtpManager::PerformAction(const SmtpManager::Action& p_Action)
{
  Result result;
  result.m_Action = p_Action;

  const std::vector<Contact> to = Contact::FromStrings(Util::SplitAddrsUnquote(p_Action.m_To));
  const std::vector<Contact> cc = Contact::FromStrings(Util::SplitAddrsUnquote(p_Action.m_Cc));
  const std::vector<Contact> bcc = Contact::FromStrings(Util::SplitAddrsUnquote(p_Action.m_Bcc));
  const std::string& ref = p_Action.m_RefMsgId;
  const std::vector<std::string> att = Util::SplitPaths(p_Action.m_Att);
  const bool flow = p_Action.m_FormatFlowed;

  Smtp smtp(m_User, m_Pass, m_Host, m_Port, m_Name, m_Address, m_Timeout);

  if (p_Action.m_IsSendMessage)
  {
    SetStatus(Status::FlagSending);
    result.m_Result = smtp.Send(p_Action.m_Subject, p_Action.m_Body, p_Action.m_HtmlBody,
                                to, cc, bcc, ref, att, flow, result.m_Message);
    ClearStatus(Status::FlagSending);
  }
  else if (p_Action.m_IsCreateMessage)
  {
    const std::string& header = smtp.GetHeader(p_Action.m_Subject, to, cc, bcc, ref);
    const std::string& body = smtp.GetBody(p_Action.m_Body, p_Action.m_HtmlBody, att, false);
    result.m_Message = header + body;
    result.m_Result = !result.m_Message.empty();
  }
  else if (p_Action.m_IsSendCreatedMessage)
  {
    SetStatus(Status::FlagSending);
    result.m_Result = smtp.Send(p_Action.m_CreatedMsg, to, cc, bcc);
    ClearStatus(Status::FlagSending);
  }
  else
  {
    LOG_WARNING("unknown action");
  }

  return result;
}

void SmtpManager::SetStatus(uint32_t p_Flags)
{
  StatusUpdate statusUpdate;
  statusUpdate.SetFlags = p_Flags;
  if (m_StatusHandler)
  {
    m_StatusHandler(statusUpdate);
  }
}

void SmtpManager::ClearStatus(uint32_t p_Flags)
{
  StatusUpdate statusUpdate;
  statusUpdate.ClearFlags = p_Flags;
  if (m_StatusHandler)
  {
    m_StatusHandler(statusUpdate);
  }
}
