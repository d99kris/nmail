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
#include <netinet/in.h>
#include <sys/socket.h>

#include "libetpan_help.h"
#include <libetpan/mailimap.h>

#include "crypto.h"
#include "strutil.h"

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
  return err.empty() ? StrUtil::Join(ips, ", ") : err;
}

std::string ImapUtil::GetImapResponseStr(struct mailimap* p_Imap)
{
  return ((p_Imap != nullptr) && (p_Imap->imap_response != nullptr))
    ? std::string(p_Imap->imap_response) : std::string();
}

std::string ImapUtil::GetImapResponseCode(struct mailimap* p_Imap)
{
  // response codes not handled by libetpan, e.g. rfc 5530 AUTHENTICATIONFAILED
  return ((p_Imap != nullptr) && (p_Imap->imap_response_info != nullptr) &&
          (p_Imap->imap_response_info->rsp_atom != nullptr))
    ? std::string(p_Imap->imap_response_info->rsp_atom) : std::string();
}

std::string ImapUtil::GetImapResponseAlert(struct mailimap* p_Imap)
{
  return ((p_Imap != nullptr) && (p_Imap->imap_response_info != nullptr) &&
          (p_Imap->imap_response_info->rsp_alert != nullptr))
    ? std::string(p_Imap->imap_response_info->rsp_alert) : std::string();
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

std::string ImapUtil::GetSpecialUseRole(const std::string& p_Attr)
{
  // rfc 6154 special-use and legacy xlist attributes, without leading backslash
  static const std::map<std::string, std::string> attrRoles =
  {
    { "sent", "sent" },
    { "drafts", "drafts" },
    { "trash", "trash" },
    { "junk", "spam" },
    { "spam", "spam" },
  };

  auto it = attrRoles.find(AsciiToLower(p_Attr));
  return (it != attrRoles.end()) ? it->second : std::string();
}

std::string ImapUtil::GetSpecialNameRole(const std::string& p_Folder, char p_Delim)
{
  // common folder names, ascii in lower case, non-ascii in typical case
  static const std::map<std::string, std::string> nameRoles = []()
  {
    static const std::map<std::string, std::vector<std::string>> roleNames =
    {
      { "sent",
        { "sent", "sent items", "sent mail", "sent messages", "sent email",
          "gesendet", "gesendete elemente", "gesendete objekte",
          "envoyés", "Éléments envoyés", "éléments envoyés", "messages envoyés",
          "enviados", "elementos enviados", "itens enviados", "inviati", "posta inviata",
          "verzonden", "verzonden items", "verzonden berichten", "skickat", "skickade objekt",
          "sendt", "sendte elementer", "lähetetyt", "wysłane", "odeslané", "elküldött",
          "Отправленные", "отправленные", "Απεσταλμένα", "已发送", "已發送", "送信済み", "보낸편지함" } },
      { "drafts",
        { "drafts", "draft", "Entwürfe", "entwürfe", "brouillons", "borradores", "rascunhos",
          "bozze", "concepten", "utkast", "kladder", "luonnokset", "kopie robocze", "koncepty",
          "piszkozatok", "Черновики", "черновики", "Πρόχειρα", "草稿", "下書き", "임시보관함" } },
      { "trash",
        { "trash", "deleted", "deleted items", "deleted messages", "bin", "rubbish",
          "papierkorb", "gelöschte elemente", "gelöschte objekte",
          "corbeille", "Éléments supprimés", "éléments supprimés",
          "papelera", "elementos eliminados", "lixeira", "lixo", "itens excluídos",
          "cestino", "posta eliminata", "prullenbak", "verwijderde items",
          "papperskorgen", "borttagna objekt", "papirkurv", "slettede elementer",
          "roskakori", "poistetut", "kosz", "usunięte", "koš",
          "Корзина", "корзина", "Удаленные", "удаленные", "Κάδος απορριμμάτων",
          "已删除", "已刪除", "ゴミ箱", "휴지통" } },
      { "spam",
        { "spam", "junk", "junk e-mail", "junk email", "junk mail", "bulk", "bulk mail",
          "spamverdacht", "courrier indésirable", "indésirables", "correo no deseado",
          "lixo eletrônico", "posta indesiderata", "ongewenste e-mail", "skräppost",
          "søppelpost", "uønsket e-post", "roskaposti",
          "Спам", "спам", "垃圾邮件", "迷惑メール", "스팸메일함" } },
    };

    std::map<std::string, std::string> map;
    for (const auto& roleName : roleNames)
    {
      for (const auto& name : roleName.second)
      {
        map[AsciiToLower(name)] = roleName.first;
      }
    }

    return map;
  }();

  // only consider top-level folders, also when nested under inbox
  std::string name = p_Folder;
  if (p_Delim != '\0')
  {
    const std::string inboxPrefix = std::string("inbox") + p_Delim;
    if (StrUtil::StartsWith(AsciiToLower(name), inboxPrefix))
    {
      name = name.substr(inboxPrefix.size());
    }

    if (name.find(p_Delim) != std::string::npos) return std::string();
  }

  auto it = nameRoles.find(AsciiToLower(name));
  return (it != nameRoles.end()) ? it->second : std::string();
}

std::string ImapUtil::AsciiToLower(const std::string& p_Str)
{
  std::string lower = p_Str;
  for (auto& ch : lower)
  {
    if ((ch >= 'A') && (ch <= 'Z'))
    {
      ch = ch - 'A' + 'a';
    }
  }

  return lower;
}

int ImapUtil::GetImapFd(struct mailimap* p_Imap)
{
  if ((p_Imap == nullptr) || (p_Imap->imap_stream == nullptr)) return -1;

  mailstream_low* low = mailstream_get_low(p_Imap->imap_stream);
  if (low == nullptr) return -1;

  return mailstream_low_get_fd(low);
}
