// imaputil.h
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct mailimap;
struct sockaddr_storage;

class ImapUtil
{
public:
  static std::string GetConnectionAddresses(struct mailimap* p_Imap);
  static std::string GetExchangeServerId(const std::string& p_Response);
  static std::string GetHostAddresses(const std::string& p_Host);
  static std::string GetImapResponseStr(struct mailimap* p_Imap);
  static std::string GetPeerIp(struct mailimap* p_Imap);

  static int64_t GetTimeMs();
  static std::vector<std::string> ResolveHostIps(const std::string& p_Host, std::string& p_Err);
  static std::string TokenFingerprint(const std::string& p_Token);

private:
  static std::string Base64Decode(const std::string& p_Str);
  static std::string SockAddrToIp(const struct sockaddr_storage& p_Addr);
  static std::string SockAddrToString(const struct sockaddr_storage& p_Addr);
  static std::string Utf16LeToAscii(const std::string& p_Str);

  static int GetImapFd(struct mailimap* p_Imap);
};
