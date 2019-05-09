// smtpmanager.cpp
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "smtpmanager.h"

#include <sys/ioctl.h>

#include "loghelp.h"
#include "smtp.h"

SmtpManager::SmtpManager(const std::string &p_User, const std::string &p_Pass,
                         const std::string &p_Host, const uint16_t p_Port,
                         const std::string &p_Name, const std::string &p_Address,
                         const bool p_Connect,
                         const std::function<void (const SmtpManager::Result &)> &p_ResultHandler,
                         const std::function<void (const StatusUpdate &)> &p_StatusHandler)
  : m_User(p_User)
  , m_Pass(p_Pass)
  , m_Host(p_Host)
  , m_Port(p_Port)
  , m_Name(p_Name)
  , m_Address(p_Address)
  , m_Connect(p_Connect)
  , m_ResultHandler(p_ResultHandler)
  , m_StatusHandler(p_StatusHandler)
  , m_Running(false)
{
  pipe(m_Pipe);
  m_Running = true;
  m_Thread = std::thread(&SmtpManager::Process, this);
}

SmtpManager::~SmtpManager()
{
  LOG_DEBUG("smtp manager running flag set false");  
  m_Running = false;
  write(m_Pipe[1], "1", 1);
  m_Thread.join();
  LOG_DEBUG("smtp manager thread joined");  
  close(m_Pipe[0]);
  close(m_Pipe[1]);
  LOG_DEBUG("smtp manager destroyed");
}

void SmtpManager::AsyncAction(const SmtpManager::Action &p_Action)
{
  if (m_Connect)
  {
    m_Actions.push_front(p_Action);
    write(m_Pipe[1], "1", 1);
  }
  else
  {
    LOG_WARNING("action not permitted while offline");
  }
}

std::string SmtpManager::GetAddress()
{
  return m_Address;
}

void SmtpManager::Process()
{
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
        read(m_Pipe[0], &buf[0], len);
      }

      m_QueueMutex.lock();

      while (m_Running && (!m_Actions.empty()))
      {
        while (!m_Actions.empty())
        {
          const Action action = m_Actions.front();
          m_Actions.pop_front();
          m_QueueMutex.unlock();

          PerformAction(action);

          m_QueueMutex.lock();
        }
      }

      m_QueueMutex.unlock();
    }
  }
}

void SmtpManager::PerformAction(const SmtpManager::Action &p_Action)
{
  Result result;

  SetStatus(Status::FlagSending);

  const std::vector<Contact> to = Contact::FromStrings(Util::Split(p_Action.m_To));
  const std::vector<Contact> cc = Contact::FromStrings(Util::Split(p_Action.m_Cc));
  const std::vector<Contact> bcc; // @todo: = Contact::FromStrings(Util::Split(p_Action.m_Bcc));
  const std::vector<std::string> att = Util::Split(p_Action.m_Att);

  Smtp smtp(m_User, m_Pass, m_Host, m_Port, m_Name, m_Address);
  result.m_Result = smtp.Send(p_Action.m_Subject, p_Action.m_Body, to, cc, bcc, att);

  ClearStatus(Status::FlagSending);

  if (m_ResultHandler)
  {
    m_ResultHandler(result);
  }
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
