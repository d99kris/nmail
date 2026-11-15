// provider.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "provider.h"

#include <algorithm>

#ifdef HAVE_RES_QUERY
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#endif

#include "config.h"
#include "loghelp.h"
#include "providers.h"
#include "util.h"

static const size_t providerCount = sizeof(EmbeddedProviders) / sizeof(EmbeddedProviders[0]);

static const std::map<std::string, std::map<std::string, std::string>>& GetProviders()
{
  static const std::map<std::string, std::map<std::string, std::string>> providers = []()
  {
    std::map<std::string, std::map<std::string, std::string>> map;
    for (size_t i = 0; i < providerCount; ++i)
    {
      const EmbeddedProvider& provider = EmbeddedProviders[i];
      Config config;
      config.LoadData(std::string(reinterpret_cast<const char*>(provider.data), provider.size));
      map[provider.id] = config.GetMap();
    }

    return map;
  }();

  return providers;
}

static std::vector<std::string> GetList(const std::map<std::string, std::string>& p_Params,
                                        const std::string& p_Key)
{
  std::vector<std::string> list;
  auto it = p_Params.find(p_Key);
  if (it == p_Params.end()) return list;

  for (const auto& item : Util::Split(it->second, ','))
  {
    const std::string value = Util::ToLower(Util::Trim(item));
    if (!value.empty())
    {
      list.push_back(value);
    }
  }

  return list;
}

std::vector<std::string> Provider::GetIds()
{
  std::vector<std::string> ids;
  for (const auto& provider : GetProviders())
  {
    ids.push_back(provider.first);
  }

  return ids;
}

bool Provider::Exists(const std::string& p_Id)
{
  return (GetProviders().count(p_Id) > 0);
}

std::map<std::string, std::string> Provider::GetParams(const std::string& p_Id)
{
  auto it = GetProviders().find(p_Id);
  return (it != GetProviders().end()) ? it->second : std::map<std::string, std::string>();
}

std::string Provider::FindByDomain(const std::string& p_Domain)
{
  const std::string domain = Util::ToLower(p_Domain);
  for (const auto& provider : GetProviders())
  {
    const std::vector<std::string> domains = GetList(provider.second, "domains");
    if (std::find(domains.begin(), domains.end(), domain) != domains.end())
    {
      return provider.first;
    }
  }

  return std::string();
}

std::string Provider::FindByMx(const std::string& p_Domain, MxResult& p_MxResult)
{
  if (!HasMxLookup())
  {
    p_MxResult = MxResultUnavailable;
    return std::string();
  }

  std::vector<std::string> hosts;
  if (!GetMxHosts(p_Domain, hosts))
  {
    p_MxResult = MxResultFailed;
    return std::string();
  }

  for (const auto& host : hosts)
  {
    // longest match wins, e.g. mx-aol.mail.gm0.yahoodns.net over yahoodns.net
    std::string matchId;
    size_t matchLen = 0;
    for (const auto& provider : GetProviders())
    {
      for (const auto& mx : GetList(provider.second, "mx"))
      {
        if ((mx.size() > matchLen) && IsDomainOrSubdomain(host, mx))
        {
          matchId = provider.first;
          matchLen = mx.size();
        }
      }
    }

    if (!matchId.empty())
    {
      p_MxResult = MxResultMatch;
      return matchId;
    }
  }

  p_MxResult = MxResultNoMatch;
  return std::string();
}

bool Provider::HasMxLookup()
{
#ifdef HAVE_RES_QUERY
  return true;
#else
  return false;
#endif
}

bool Provider::GetMxHosts(const std::string& p_Domain, std::vector<std::string>& p_Hosts)
{
#ifdef HAVE_RES_QUERY
  static const int classIn = 1;
  static const int typeMx = 15;
  static const int headerSize = 12;

  unsigned char buf[4096];
  const int len = res_query(p_Domain.c_str(), classIn, typeMx, buf, sizeof(buf));
  if (len < 0)
  {
    // non-existent domain or no mx records is not a lookup failure
    const int err = h_errno;
    LOG_DEBUG("mx lookup h_errno %d", err);
    return ((err == HOST_NOT_FOUND) || (err == NO_DATA));
  }

  if ((len < headerSize) || (len > (int)sizeof(buf)))
  {
    LOG_DEBUG("mx lookup invalid response length %d", len);
    return false;
  }

  const unsigned char* end = buf + len;
  const int qdCount = (buf[4] << 8) | buf[5];
  const int anCount = (buf[6] << 8) | buf[7];
  const unsigned char* pos = buf + headerSize;
  char name[NS_MAXDNAME];

  // skip question section
  for (int i = 0; i < qdCount; ++i)
  {
    const int nameLen = dn_expand(buf, end, pos, name, sizeof(name));
    if ((nameLen < 0) || ((end - pos) < (nameLen + 4))) return false;

    pos += nameLen + 4;
  }

  // parse answer section
  std::vector<std::pair<int, std::string>> prefHosts;
  for (int i = 0; i < anCount; ++i)
  {
    const int nameLen = dn_expand(buf, end, pos, name, sizeof(name));
    if ((nameLen < 0) || ((end - pos) < (nameLen + 10))) break;

    pos += nameLen;
    const int type = (pos[0] << 8) | pos[1];
    const int dataLen = (pos[8] << 8) | pos[9];
    pos += 10;
    if ((end - pos) < dataLen) break;

    if ((type == typeMx) && (dataLen > 2))
    {
      const int pref = (pos[0] << 8) | pos[1];
      if (dn_expand(buf, end, pos + 2, name, sizeof(name)) >= 0)
      {
        prefHosts.push_back(std::make_pair(pref, Util::ToLower(name)));
      }
    }

    pos += dataLen;
  }

  std::stable_sort(prefHosts.begin(), prefHosts.end(),
                   [](const std::pair<int, std::string>& p_Lhs, const std::pair<int, std::string>& p_Rhs)
  {
    return p_Lhs.first < p_Rhs.first;
  });

  for (const auto& prefHost : prefHosts)
  {
    p_Hosts.push_back(prefHost.second);
  }

  LOG_DEBUG("mx lookup hosts %zu", p_Hosts.size());
  return true;
#else
  (void)p_Domain;
  (void)p_Hosts;
  return false;
#endif
}

bool Provider::IsDomainOrSubdomain(const std::string& p_Host, const std::string& p_Domain)
{
  std::string host = p_Host;
  if (!host.empty() && (host.back() == '.'))
  {
    host.pop_back();
  }

  if (host == p_Domain) return true;

  const std::string suffix = "." + p_Domain;
  return (host.size() > suffix.size()) &&
         (host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0);
}
