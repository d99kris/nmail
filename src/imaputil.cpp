// imaputil.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "imaputil.h"

#include <cerrno>
#include <chrono>
#include <cstring>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

#include "libetpan_help.h"
#include <libetpan/mailimap.h>

#include "crypto.h"
#include "util.h"

std::string ImapUtil::GetConnectionAddresses(struct mailimap* p_Imap)
{
  int fd = GetImapFd(p_Imap);
  if (fd == -1) return "no fd";

  struct sockaddr_storage local;
  struct sockaddr_storage peer;
  socklen_t localLen = sizeof(local);
  socklen_t peerLen = sizeof(peer);
  const std::string localStr = (getsockname(fd, (struct sockaddr*)&local, &localLen) == 0)
    ? SockAddrToString(local) : std::string(strerror(errno));
  const std::string peerStr = (getpeername(fd, (struct sockaddr*)&peer, &peerLen) == 0)
    ? SockAddrToString(peer) : std::string(strerror(errno));
  return "local " + localStr + " peer " + peerStr;
}

// parses the base64-encoded utf-16 server id that Exchange Online appends to
// greetings/responses, ex:
// "* OK ... service ready. <guid> (tcpproxy/... BACKENDAUTHENTICATE) [UwBJADIA...]"
// decoding it identifies which frontend/backend served the session
std::string ImapUtil::GetExchangeServerId(const std::string& p_Response)
{
  size_t end = p_Response.rfind(']');
  if (end == std::string::npos) return std::string();

  size_t start = p_Response.rfind('[', end);
  if (start == std::string::npos) return std::string();

  return Utf16LeToAscii(Base64Decode(p_Response.substr(start + 1, end - start - 1)));
}

std::string ImapUtil::GetHostAddresses(const std::string& p_Host)
{
  std::string err;
  const std::vector<std::string> ips = ResolveHostIps(p_Host, err);
  return err.empty() ? Util::Join(ips, ", ") : err;
}

std::string ImapUtil::GetImapResponseStr(struct mailimap* p_Imap)
{
  return ((p_Imap != nullptr) && (p_Imap->imap_response != nullptr))
    ? std::string(p_Imap->imap_response) : std::string();
}

std::string ImapUtil::GetPeerIp(struct mailimap* p_Imap)
{
  int fd = GetImapFd(p_Imap);
  if (fd == -1) return std::string();

  struct sockaddr_storage peer;
  socklen_t peerLen = sizeof(peer);
  if (getpeername(fd, (struct sockaddr*)&peer, &peerLen) != 0) return std::string();

  return SockAddrToIp(peer);
}

int64_t ImapUtil::GetTimeMs()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::vector<std::string> ImapUtil::ResolveHostIps(const std::string& p_Host, std::string& p_Err)
{
  p_Err.clear();
  std::vector<std::string> ips;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* result = nullptr;
  int rv = getaddrinfo(p_Host.c_str(), nullptr, &hints, &result);
  if (rv != 0)
  {
    p_Err = std::string("getaddrinfo err: ") + gai_strerror(rv);
    return ips;
  }

  for (struct addrinfo* ai = result; ai != nullptr; ai = ai->ai_next)
  {
    char buf[INET6_ADDRSTRLEN] = { 0 };
    const void* src = nullptr;
    if (ai->ai_family == AF_INET)
    {
      src = &((const struct sockaddr_in*)(const void*)ai->ai_addr)->sin_addr;
    }
    else if (ai->ai_family == AF_INET6)
    {
      src = &((const struct sockaddr_in6*)(const void*)ai->ai_addr)->sin6_addr;
    }
    else
    {
      continue;
    }

    if (inet_ntop(ai->ai_family, src, buf, sizeof(buf)) != nullptr)
    {
      ips.push_back(buf);
    }
  }

  freeaddrinfo(result);
  return ips;
}

// short non-reversible token identifier for correlating log entries, not a secret
std::string ImapUtil::TokenFingerprint(const std::string& p_Token)
{
  if (p_Token.empty()) return "empty";

  return Crypto::SHA256(p_Token).substr(0, 8);
}

std::string ImapUtil::Base64Decode(const std::string& p_Str)
{
  // handles both standard and url-safe alphabet, ignores padding/whitespace
  auto decodeChar = [](char c) -> int
  {
    if ((c >= 'A') && (c <= 'Z')) return (c - 'A');
    if ((c >= 'a') && (c <= 'z')) return (c - 'a') + 26;
    if ((c >= '0') && (c <= '9')) return (c - '0') + 52;
    if ((c == '+') || (c == '-')) return 62;
    if ((c == '/') || (c == '_')) return 63;
    return -1;
  };

  std::string dec;
  int bits = 0;
  int bitCount = 0;
  for (char c : p_Str)
  {
    int val = decodeChar(c);
    if (val == -1)
    {
      if ((c == '=') || (c == '\r') || (c == '\n')) continue;

      return std::string();
    }

    bits = (bits << 6) | val;
    bitCount += 6;
    if (bitCount >= 8)
    {
      bitCount -= 8;
      dec.push_back((char)((bits >> bitCount) & 0xff));
    }
  }

  return dec;
}

std::string ImapUtil::SockAddrToIp(const struct sockaddr_storage& p_Addr)
{
  char buf[INET6_ADDRSTRLEN] = { 0 };
  if (p_Addr.ss_family == AF_INET)
  {
    const struct sockaddr_in* sin = (const struct sockaddr_in*)&p_Addr;
    inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
  }
  else if (p_Addr.ss_family == AF_INET6)
  {
    const struct sockaddr_in6* sin6 = (const struct sockaddr_in6*)&p_Addr;
    inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf));
  }

  return std::string(buf);
}

std::string ImapUtil::SockAddrToString(const struct sockaddr_storage& p_Addr)
{
  uint16_t port = 0;
  if (p_Addr.ss_family == AF_INET)
  {
    port = ntohs(((const struct sockaddr_in*)&p_Addr)->sin_port);
  }
  else if (p_Addr.ss_family == AF_INET6)
  {
    port = ntohs(((const struct sockaddr_in6*)&p_Addr)->sin6_port);
  }
  else
  {
    return "family " + std::to_string(p_Addr.ss_family);
  }

  return SockAddrToIp(p_Addr) + ":" + std::to_string(port);
}

std::string ImapUtil::Utf16LeToAscii(const std::string& p_Str)
{
  if ((p_Str.size() % 2) != 0) return std::string();

  std::string ascii;
  for (size_t i = 0; i < p_Str.size(); i += 2)
  {
    unsigned char lo = p_Str[i];
    unsigned char hi = p_Str[i + 1];
    if ((hi != 0) || (lo < 0x20) || (lo > 0x7e)) return std::string();

    ascii.push_back((char)lo);
  }

  return ascii;
}

int ImapUtil::GetImapFd(struct mailimap* p_Imap)
{
  if ((p_Imap == nullptr) || (p_Imap->imap_stream == nullptr)) return -1;

  mailstream_low* low = mailstream_get_low(p_Imap->imap_stream);
  if (low == nullptr) return -1;

  return mailstream_low_get_fd(low);
}
