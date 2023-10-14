// sasl.cpp
//
// Copyright (c) 2020-2023 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "sasl.h"

#include <libetpan/mailsmtp_types.h>
#include <sasl/sasl.h>

#include "loghelp.h"

// Ideally Cyrus SASL should not be a direct dependency of nmail (it's an
// indirect dependency through libetpan), however there's been quite a bit
// of user-reported bugs with failure to authenticate SMTP due to missing
// the LOGIN mechanism. With this file we add direct interfacing with SASL
// to get a list of its installed mechanisms. The list retrieved and logged
// at nmail startup.

// Hack to disable warning about macOS deprecation of SASL functions.
#ifdef __APPLE__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

std::string Sasl::GetMechanismsStr()
{
  std::string mechanismsStr;
  std::set<std::string> mechanisms = GetMechanisms();
  for (const auto& mechanism : mechanisms)
  {
    mechanismsStr += std::string((mechanismsStr.empty()) ? "" : ", ") + mechanism;
  }

  return mechanismsStr;
}

bool Sasl::IsMechanismsSupported(int p_Auths /* contains flags MAILSMTP_AUTH_* */)
{
  bool isMechanismsSupported =
    IsRequestedMechanismSupported(p_Auths, MAILSMTP_AUTH_LOGIN, "LOGIN") &&
    IsRequestedMechanismSupported(p_Auths, MAILSMTP_AUTH_CRAM_MD5, "CRAM_MD5") &&
    IsRequestedMechanismSupported(p_Auths, MAILSMTP_AUTH_PLAIN, "PLAIN") &&
    IsRequestedMechanismSupported(p_Auths, MAILSMTP_AUTH_DIGEST_MD5, "DIGEST_MD5") &&
    IsRequestedMechanismSupported(p_Auths, MAILSMTP_AUTH_GSSAPI, "GSSAPI") &&
    IsRequestedMechanismSupported(p_Auths, MAILSMTP_AUTH_SRP, "SRP") &&
    IsRequestedMechanismSupported(p_Auths, MAILSMTP_AUTH_NTLM, "NTLM") &&
    IsRequestedMechanismSupported(p_Auths, MAILSMTP_AUTH_KERBEROS_V4, "KERBEROS_V4");

  return isMechanismsSupported;
}

std::set<std::string> Sasl::GetMechanisms()
{
  static const std::set<std::string> s_Mechanisms = []()
  {
    std::set<std::string> mechanisms;

    if (LOG_IF_NOT_EQUAL(sasl_client_init(NULL), SASL_OK) != SASL_OK)
    {
      return std::set<std::string>();
    }

    const char** mechs = sasl_global_listmech();
    for (int i = 0; (mechs != NULL) && (mechs[i] != NULL); ++i)
    {
      std::string mechstr = std::string(mechs[i]);
      std::transform(mechstr.begin(), mechstr.end(), mechstr.begin(), ::toupper);
      mechanisms.insert(mechstr);
    }

    sasl_client_done();

    return mechanisms;
  }();

  return s_Mechanisms;
}

bool Sasl::IsRequestedMechanismSupported(int p_Auths, int p_ReqAuth, const std::string& p_AuthStr)
{
  if (p_Auths & p_ReqAuth)
  {
    const std::set<std::string>& mechanisms = GetMechanisms();
    bool isSupported = mechanisms.count(p_AuthStr);
    if (!isSupported)
    {
      LOG_ERROR("sasl auth mechanism %s not available", p_AuthStr.c_str());
    }

    return isSupported;
  }
  else
  {
    return true;
  }
}

#ifdef __APPLE__
#pragma GCC diagnostic pop
#endif
