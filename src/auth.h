// auth.h
//
// Copyright (c) 2021 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <map>
#include <mutex>
#include <string>

class Auth
{
public:
  static void Init(const std::string& p_Auth, const bool p_AuthEncrypt,
                   const std::string& p_Pass, const bool p_IsSetup);
  static void Cleanup();

  static bool ChangePass(const bool p_CacheEncrypt,
                         const std::string& p_OldPass, const std::string& p_NewPass);

  static bool GenerateToken(const std::string& p_Auth);
  static std::string GetName();
  static std::string GetEmail();
  static std::string GetAccessToken();
  static bool IsOAuthEnabled();
  static bool RefreshNeeded();
  static bool RefreshToken();
  static int64_t GetTimeToExpirySec();

private:
  enum AuthAction { Generate, Refresh };

  static void InitCacheDir();
  static std::string GetAuthCacheDir();
  static std::string GetAuthTempDir();
  static void LoadCache();
  static void SaveCache();
  static std::string GetTokenStoreTempPath();
  static std::string GetClientId();
  static std::string GetClientSecret();
  static std::map<std::string, std::string> GetDefaultTokens();
  static int64_t GetCurrentTimeSec();
  static void UpdateExpiryTime();
  static int PerformAction(const AuthAction p_AuthAction);

private:
  static std::mutex m_Mutex;

  static std::string m_Auth;
  static std::string m_Pass;
  static bool m_AuthEncrypt;
  static bool m_OAuthEnabled;
  static int64_t m_ExpiryTime;

  static std::string m_CustomClientId;
  static std::string m_CustomClientSecret;
};
