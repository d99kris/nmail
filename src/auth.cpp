// auth.cpp
//
// Copyright (c) 2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "auth.h"

#include <cstdlib>
#include <string>

#include "cacheutil.h"
#include "config.h"
#include "log.h"
#include "loghelp.h"
#include "util.h"

std::mutex Auth::m_Mutex;
std::string Auth::m_Auth;
std::string Auth::m_Pass;
bool Auth::m_AuthEncrypt = false;
bool Auth::m_OAuthEnabled = false;
int64_t Auth::m_ExpiryTime = 0;
std::string Auth::m_CustomClientId;
std::string Auth::m_CustomClientSecret;

void Auth::Init(const std::string& p_Auth, const bool p_AuthEncrypt,
                const std::string& p_Pass, const bool p_IsSetup)
{
  LOG_DEBUG_FUNC(STR(p_Auth, p_AuthEncrypt, "***", p_IsSetup));

  std::lock_guard<std::mutex> lock(m_Mutex);
  m_Auth = p_Auth;
  m_AuthEncrypt = p_AuthEncrypt;
  m_Pass = p_Pass;
  m_OAuthEnabled = (m_Auth == "gmail-oauth2");

  if (!m_OAuthEnabled) return;

  InitCacheDir();

  // read custom auth config
  const std::map<std::string, std::string> defaultConfig =
  {
    { "oauth2_client_id", "" },
    { "oauth2_client_secret", "" },
  };
  const std::string configPath(Util::GetApplicationDir() + std::string("auth.conf"));
  Config config(configPath, defaultConfig);
  config.LogParams();
  m_CustomClientId = config.Get("oauth2_client_id");
  m_CustomClientSecret = config.Get("oauth2_client_secret");

  if (!p_IsSetup)
  {
    LoadCache();
  }
  else
  {
    SaveCache(); // perform save in case of unexpected termination
  }
}

void Auth::Cleanup()
{
  LOG_DEBUG_FUNC(STR());

  std::lock_guard<std::mutex> lock(m_Mutex);
  SaveCache();
  Util::RmDir(GetAuthTempDir());
}

bool Auth::GenerateToken(const std::string& p_Auth)
{
  LOG_DEBUG_FUNC(STR(p_Auth));

  std::lock_guard<std::mutex> lock(m_Mutex);
  m_Auth = p_Auth;
  Util::RmDir(GetAuthTempDir());
  Util::MkDir(GetAuthTempDir());

  m_OAuthEnabled = (m_Auth == "gmail-oauth2");

  if (!m_OAuthEnabled) return false;

  InitCacheDir();

  int status = PerformAction(Generate);

  return (WIFEXITED(status) && (WEXITSTATUS(status) == 0));
}

std::string Auth::GetName()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  Config tokens(GetTokenStoreTempPath(), GetDefaultTokens());
  return tokens.Get("name");
}

std::string Auth::GetEmail()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  Config tokens(GetTokenStoreTempPath(), GetDefaultTokens());
  return tokens.Get("email");
}

std::string Auth::GetAccessToken()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  Config tokens(GetTokenStoreTempPath(), GetDefaultTokens());
  return tokens.Get("access_token");
}

bool Auth::IsOAuthEnabled()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  return m_OAuthEnabled;
}

bool Auth::RefreshNeeded()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  if (!m_OAuthEnabled) return false;

  if (GetTimeToExpirySec() > 0) return false;

  static int64_t lastRefreshNeeded = 0;
  int64_t currentTime = GetCurrentTimeSec();
  static const int64_t minRefreshInterval = 30;

  if (currentTime < (lastRefreshNeeded + minRefreshInterval)) return false;

  lastRefreshNeeded = currentTime;
  return true;
}

bool Auth::RefreshToken()
{
  LOG_DEBUG_FUNC(STR());

  std::lock_guard<std::mutex> lock(m_Mutex);
  if (!m_OAuthEnabled) return false;

  int status = PerformAction(Refresh);

  return (WIFEXITED(status) && (WEXITSTATUS(status) == 0));
}

void Auth::InitCacheDir()
{
  static const int version = 1;
  const std::string cacheDir = GetAuthCacheDir();
  CacheUtil::CommonInitCacheDir(cacheDir, version, m_AuthEncrypt);
}

std::string Auth::GetAuthCacheDir()
{
  return CacheUtil::GetCacheDir() + std::string("auth/");
}

std::string Auth::GetAuthTempDir()
{
  return Util::GetTempDir() + std::string("auth/");
}

void Auth::LoadCache()
{
  Util::RmDir(GetAuthTempDir());
  Util::MkDir(GetAuthTempDir());
  if (m_AuthEncrypt)
  {
    CacheUtil::DecryptCacheDir(m_Pass, GetAuthCacheDir(), GetAuthTempDir());
  }
  else
  {
    Util::CopyFiles(GetAuthCacheDir(), GetAuthTempDir());
  }
}

void Auth::SaveCache()
{
  if (m_AuthEncrypt)
  {
    CacheUtil::EncryptCacheDir(m_Pass, GetAuthTempDir(), GetAuthCacheDir());
  }
  else
  {
    Util::CopyFiles(GetAuthTempDir(), GetAuthCacheDir());
  }
}

std::string Auth::GetTokenStoreTempPath()
{
  return GetAuthTempDir() + m_Auth + std::string(".tokens");
}

std::string Auth::GetClientId()
{
  if (!m_CustomClientId.empty())
  {
    return m_CustomClientId;
  }
  else
  {
    return Util::FromHex("3639393831313539393539322D6338697569646B743963663773347034"
                         "646376726B636A747136687269346F702E617070732E676F6F676C6575"
                         "736572636F6E74656E742E636F6D");
  }
}

std::string Auth::GetClientSecret()
{
  if (!m_CustomClientSecret.empty())
  {
    return m_CustomClientSecret;
  }
  else
  {
    return Util::FromHex("6A79664B785F67683536537377486A5952764A4C32564A77");
  }
}

std::map<std::string, std::string> Auth::GetDefaultTokens()
{
  static const std::map<std::string, std::string> defaultTokens =
  {
    { "name", "" },
    { "email", "" },
    { "access_token", "" },
    { "expires_in", "0" },
  };
  return defaultTokens;
}

int64_t Auth::GetCurrentTimeSec()
{
  struct timeval now;
  gettimeofday(&now, NULL);
  return static_cast<int64_t>(now.tv_sec);
}

void Auth::UpdateExpiryTime()
{
  Config tokens(GetTokenStoreTempPath(), GetDefaultTokens());
  std::string expiresInStr = tokens.Get("expires_in");
  static const int64_t marginTime = 60; // renew slightly before expiry
  int64_t expiresIn = strtoll(expiresInStr.c_str(), NULL, 10) - marginTime;
  m_ExpiryTime = GetCurrentTimeSec() + expiresIn;
  LOG_DEBUG("oauth2 expires in %d sec", expiresIn);
}

int64_t Auth::GetTimeToExpirySec()
{
  return (m_ExpiryTime - GetCurrentTimeSec());
}

int Auth::PerformAction(const AuthAction p_AuthAction)
{
  static const std::string type = m_Auth;
  static const std::string clientId = GetClientId();
  static const std::string clientSecret = GetClientSecret();
  static const std::string tokenStore = GetTokenStoreTempPath();
  static const std::string scriptPath = Util::DirName(Util::GetSelfPath()) + "/oauth2nmail";

  setenv("OAUTH2_TYPE", type.c_str(), 1);
  setenv("OAUTH2_CLIENT_ID", clientId.c_str(), 1);
  setenv("OAUTH2_CLIENT_SECRET", clientSecret.c_str(), 1);
  setenv("OAUTH2_TOKEN_STORE", tokenStore.c_str(), 1);

  std::string outPath = Util::GetTempFilename(".txt");
  std::string command =
    scriptPath + " " + ((p_AuthAction == Generate) ? "-g" : "-r") + " > " + outPath + " 2>&1";

  int status = system(command.c_str());
  if (WIFEXITED(status) && (WEXITSTATUS(status) == 0))
  {
    LOG_DEBUG((p_AuthAction == Generate) ? "oauth2 generate ok" : "oauth2 refresh ok");
    UpdateExpiryTime();
  }
  else if (WIFEXITED(status))
  {
    LOG_WARNING((p_AuthAction == Generate) ? "oauth2 generate failed (%d): %s"
                                           : "oauth2 refresh failed (%d): %s",
                WEXITSTATUS(status), command.c_str());
    std::string output = Util::ReadFile(outPath);
    LOG_DUMP(output.c_str());
  }
  else if (WIFSIGNALED(status))
  {
    LOG_WARNING((p_AuthAction == Generate) ? "oauth2 generate killed %d"
                                           : "oauth2 refresh killed %d",
                WTERMSIG(status));
  }
  else if (WIFSTOPPED(status))
  {
    LOG_WARNING((p_AuthAction == Generate) ? "oauth2 generate stopped %d"
                                           : "oauth2 refresh stopped %d",
                WSTOPSIG(status));
  }
  else if (WIFCONTINUED(status))
  {
    LOG_WARNING((p_AuthAction == Generate) ? "oauth2 generate continued"
                                           : "oauth2 refresh continued");
  }

  Util::DeleteFile(outPath);

  return status;
}
