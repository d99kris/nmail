// offlinequeue.cpp
//
// Copyright (c) 2021-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "offlinequeue.h"

#include <string>

#include <unistd.h>

#include "apputil.h"
#include "cacheutil.h"
#include "crypto.h"
#include "fileutil.h"
#include "loghelp.h"
#include "strutil.h"
#include "sysutil.h"

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

  if (!AppUtil::GetReadOnly())
  {
    std::vector<std::string> composeMsgs = PopComposeMessages();
    for (const auto& composeMsg : composeMsgs)
    {
      PushDraftMessage(composeMsg);
    }
  }
}

void OfflineQueue::Cleanup()
{
}

bool OfflineQueue::ChangePass(const bool p_CacheEncrypt,
                              const std::string& p_OldPass, const std::string& p_NewPass)
{
  if (!p_CacheEncrypt) return true;

  // @todo: implement password change for offline queue
  (void)p_OldPass;
  (void)p_NewPass;
  return true;
}

std::string OfflineQueue::GetQueueFileName(int p_Index)
{
  static std::string pidStr = std::to_string(getpid());
  return pidStr + "." + std::to_string(p_Index) + ".eml"; // <pid>.<idx>.eml
}

bool OfflineQueue::CanProcessFileName(const std::string& p_FileName)
{
  const std::string fileExt = FileUtil::GetFileExt(p_FileName);
  if (fileExt != ".eml") return false;

  const std::string baseName = FileUtil::RemoveFileExt(FileUtil::BaseName(p_FileName));
  const std::vector<std::string> parts = StrUtil::Split(baseName, '.');
  const std::string pidStr = !parts.empty() ? parts.at(0) : "";
  if (!StrUtil::IsInteger(pidStr))
  {
    LOG_DEBUG("unsupported filename %s", p_FileName.c_str());
    return false;
  }

  const pid_t pid = StrUtil::ToInteger(pidStr);
  if (!SysUtil::IsSelfProcess(pid) && SysUtil::IsProcessRunning(pid))
  {
    LOG_DEBUG("skip other active instance file %s", p_FileName.c_str());
    return false;
  }

  LOG_DEBUG("do process %s", p_FileName.c_str());
  return true;
}

void OfflineQueue::PushDraftMessage(const std::string& p_Str)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  int i = 0;
  while (FileUtil::Exists(GetDraftQueueDir() + GetQueueFileName(i)))
  {
    ++i;
  }

  std::string msgPath = GetDraftQueueDir() + GetQueueFileName(i);
  WriteCacheFile(msgPath, p_Str);
}

void OfflineQueue::PushOutboxMessage(const std::string& p_Str)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  int i = 0;
  while (FileUtil::Exists(GetOutboxQueueDir() + GetQueueFileName(i)))
  {
    ++i;
  }

  std::string msgPath = GetOutboxQueueDir() + GetQueueFileName(i);
  WriteCacheFile(msgPath, p_Str);
}

void OfflineQueue::PushComposeMessage(const std::string& p_Str)
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  std::string tmpPath = FileUtil::GetTempDir() + "compose.eml";
  WriteCacheFile(tmpPath, p_Str);

  int i = 0; // should only exist one active compose per process instance
  std::string msgPath = GetComposeQueueDir() + GetQueueFileName(i);
  FileUtil::Move(tmpPath, msgPath);
}

bool OfflineQueue::HasDraftMessages()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  const std::vector<std::string>& fileNames = FileUtil::ListDir(GetDraftQueueDir());
  for (auto& fileName : fileNames)
  {
    if (CanProcessFileName(fileName)) return true;
  }

  return false;
}

std::vector<std::string> OfflineQueue::PopDraftMessages()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  std::vector<std::string> msgs;
  const std::vector<std::string>& fileNames = FileUtil::ListDir(GetDraftQueueDir());
  for (auto& fileName : fileNames)
  {
    if (!CanProcessFileName(fileName)) continue;

    std::string filePath = GetDraftQueueDir() + fileName;
    std::string msg = ReadCacheFile(filePath);
    msgs.push_back(msg);
    FileUtil::DeleteFile(filePath);
  }

  return msgs;
}

std::vector<std::string> OfflineQueue::PopOutboxMessages()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  std::vector<std::string> msgs;
  const std::vector<std::string>& fileNames = FileUtil::ListDir(GetOutboxQueueDir());
  for (auto& fileName : fileNames)
  {
    if (!CanProcessFileName(fileName)) continue;

    std::string filePath = GetOutboxQueueDir() + fileName;
    std::string msg = ReadCacheFile(filePath);
    msgs.push_back(msg);
    FileUtil::DeleteFile(filePath);
  }

  return msgs;
}

std::vector<std::string> OfflineQueue::PopComposeMessages()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  std::vector<std::string> msgs;
  const std::vector<std::string>& fileNames = FileUtil::ListDir(GetComposeQueueDir());
  for (auto& fileName : fileNames)
  {
    if (!CanProcessFileName(fileName)) continue;

    std::string filePath = GetComposeQueueDir() + fileName;
    std::string msg = ReadCacheFile(filePath);
    msgs.push_back(msg);
    FileUtil::DeleteFile(filePath);
  }

  return msgs;
}

std::string OfflineQueue::GetQueueDir()
{
  return CacheUtil::GetCacheDir() + std::string("offlinequeue/");
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
    return Crypto::AESDecrypt(FileUtil::ReadFile(p_Path), m_Pass);
  }
  else
  {
    return FileUtil::ReadFile(p_Path);
  }
}

void OfflineQueue::WriteCacheFile(const std::string& p_Path, const std::string& p_Str)
{
  if (m_Encrypt)
  {
    FileUtil::WriteFile(p_Path, Crypto::AESEncrypt(p_Str, m_Pass));
  }
  else
  {
    FileUtil::WriteFile(p_Path, p_Str);
  }
}
