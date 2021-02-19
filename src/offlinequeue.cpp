// offlinequeue.cpp
//
// Copyright (c) 2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "offlinequeue.h"

#include <string>

#include "cacheutil.h"
#include "crypto.h"
#include "serialized.h"
#include "util.h"

std::mutex OfflineQueue::m_Mutex;
bool OfflineQueue::m_Encrypt = true;
std::string OfflineQueue::m_Pass;

void OfflineQueue::Init(const bool p_Encrypt, const std::string& p_Pass)
{
  m_Encrypt = p_Encrypt;
  m_Pass = p_Pass;

  InitQueueDir();
  InitDraftQueueDir();
  InitOutboxQueueDir();
  InitComposeQueueDir();

  std::vector<std::string> composeMsgs = PopComposeMessages();
  for (const auto& composeMsg : composeMsgs)
  {
    PushDraftMessage(composeMsg);
  }
}

void OfflineQueue::Cleanup()
{
}

void OfflineQueue::PushDraftMessage(const std::string& p_Str)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  int i = 0;
  while (Util::Exists(GetDraftQueueDir() + std::to_string(i) + ".eml"))
  {
    ++i;
  }

  std::string msgPath = GetDraftQueueDir() + std::to_string(i) + ".eml";
  WriteCacheFile(msgPath, p_Str);
}

void OfflineQueue::PushOutboxMessage(const std::string& p_Str)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  int i = 0;
  while (Util::Exists(GetOutboxQueueDir() + std::to_string(i) + ".eml"))
  {
    ++i;
  }

  std::string msgPath = GetOutboxQueueDir() + std::to_string(i) + ".eml";
  WriteCacheFile(msgPath, p_Str);
}

void OfflineQueue::PushComposeMessage(const std::string& p_Str)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  int i = 0;
  std::string msgPath = GetComposeQueueDir() + std::to_string(i) + ".eml";
  WriteCacheFile(msgPath, p_Str);
}

std::vector<std::string> OfflineQueue::PopDraftMessages()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  std::vector<std::string> msgs;
  const std::vector<std::string>& fileNames = Util::ListDir(GetDraftQueueDir());
  for (auto& fileName : fileNames)
  {
    const std::string& baseName = Util::RemoveFileExt(Util::BaseName(fileName));
    if (Util::IsInteger(baseName))
    {
      std::string filePath = GetDraftQueueDir() + fileName;
      std::string msg = ReadCacheFile(filePath);
      msgs.push_back(msg);
      Util::DeleteFile(filePath);
    }
  }

  return msgs;
}

std::vector<std::string> OfflineQueue::PopOutboxMessages()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  std::vector<std::string> msgs;
  const std::vector<std::string>& fileNames = Util::ListDir(GetOutboxQueueDir());
  for (auto& fileName : fileNames)
  {
    const std::string& baseName = Util::RemoveFileExt(Util::BaseName(fileName));
    if (Util::IsInteger(baseName))
    {
      std::string filePath = GetOutboxQueueDir() + fileName;
      std::string msg = ReadCacheFile(filePath);
      msgs.push_back(msg);
      Util::DeleteFile(filePath);
    }
  }

  return msgs;
}

std::vector<std::string> OfflineQueue::PopComposeMessages()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  std::vector<std::string> msgs;
  const std::vector<std::string>& fileNames = Util::ListDir(GetComposeQueueDir());
  for (auto& fileName : fileNames)
  {
    const std::string& baseName = Util::RemoveFileExt(Util::BaseName(fileName));
    if (Util::IsInteger(baseName))
    {
      std::string filePath = GetComposeQueueDir() + fileName;
      std::string msg = ReadCacheFile(filePath);
      msgs.push_back(msg);
      Util::DeleteFile(filePath);
    }
  }

  return msgs;
}

std::string OfflineQueue::GetQueueDir()
{
  return Util::GetApplicationDir() + std::string("queue/");
}

void OfflineQueue::InitQueueDir()
{
  static const int version = 1;
  const std::string queueDir = GetQueueDir();
  CacheUtil::CommonInitCacheDir(queueDir, version, m_Encrypt);
}

std::string OfflineQueue::GetDraftQueueDir()
{
  return GetQueueDir() + std::string("draft/");
}

void OfflineQueue::InitDraftQueueDir()
{
  static const int version = 1;
  const std::string draftQueueDir = GetDraftQueueDir();
  CacheUtil::CommonInitCacheDir(draftQueueDir, version, m_Encrypt);
}

std::string OfflineQueue::GetOutboxQueueDir()
{
  return GetQueueDir() + std::string("outbox/");
}

void OfflineQueue::InitOutboxQueueDir()
{
  static const int version = 1;
  const std::string outboxQueueDir = GetOutboxQueueDir();
  CacheUtil::CommonInitCacheDir(outboxQueueDir, version, m_Encrypt);
}

std::string OfflineQueue::GetComposeQueueDir()
{
  return GetQueueDir() + std::string("compose/");
}

void OfflineQueue::InitComposeQueueDir()
{
  static const int version = 1;
  const std::string composeQueueDir = GetComposeQueueDir();
  CacheUtil::CommonInitCacheDir(composeQueueDir, version, m_Encrypt);
}

std::string OfflineQueue::ReadCacheFile(const std::string& p_Path)
{
  if (m_Encrypt)
  {
    return Crypto::AESDecrypt(Util::ReadFile(p_Path), m_Pass);
  }
  else
  {
    return Util::ReadFile(p_Path);
  }
}

void OfflineQueue::WriteCacheFile(const std::string& p_Path, const std::string& p_Str)
{
  if (m_Encrypt)
  {
    Util::WriteFile(p_Path, Crypto::AESEncrypt(p_Str, m_Pass));
  }
  else
  {
    Util::WriteFile(p_Path, p_Str);
  }
}
