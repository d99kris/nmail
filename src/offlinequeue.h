// offlinequeue.h
//
// Copyright (c) 2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <mutex>
#include <string>
#include <vector>

class OfflineQueue
{
public:
  static void Init(const bool p_Encrypt, const std::string& p_Pass);
  static void Cleanup();

  static void PushDraftMessage(const std::string& p_Str);
  static void PushOutboxMessage(const std::string& p_Str);
  static void PushComposeMessage(const std::string& p_Str);
  static std::vector<std::string> PopDraftMessages();
  static std::vector<std::string> PopOutboxMessages();
  static std::vector<std::string> PopComposeMessages();

private:
  static std::string GetQueueDir();
  static void InitQueueDir();

  static std::string GetDraftQueueDir();
  static void InitDraftQueueDir();

  static std::string GetOutboxQueueDir();
  static void InitOutboxQueueDir();

  static std::string GetComposeQueueDir();
  static void InitComposeQueueDir();

  static std::string ReadCacheFile(const std::string& p_Path);
  static void WriteCacheFile(const std::string& p_Path, const std::string& p_Str);

private:
  static std::mutex m_Mutex;
  static bool m_Encrypt;
  static std::string m_Pass;
};
