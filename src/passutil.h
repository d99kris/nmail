// passutil.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <memory>
#include <string>

#include "config.h"

class PassUtil
{
public:
  static bool ObtainAuthPasswords(const bool p_IsSetup, const std::string& p_User,
                                  std::string& p_Pass, std::string& p_SmtpUser, std::string& p_SmtpPass,
                                  std::shared_ptr<Config> p_SecretConfig, std::shared_ptr<Config> p_MainConfig);
  static bool ObtainCacheEncryptPassword(const bool p_IsSetup, const std::string& p_User,
                                         bool p_SetupAllowCacheEncrypt,
                                         std::string& p_Pass, std::string& p_SmtpUser, std::string& p_SmtpPass,
                                         std::shared_ptr<Config> p_SecretConfig, std::shared_ptr<Config> p_MainConfig);
  static bool ChangePasswords(std::shared_ptr<Config> p_MainConfig,
                              std::shared_ptr<Config> p_SecretConfig);

private:
  static bool ValidatePass(const std::string& p_Pass, const std::string& p_ErrorPrefix);
  static bool ChangeCachePasswords(std::shared_ptr<Config> p_MainConfig,
                                   const std::string& p_OldPass, const std::string& p_NewPass);
};
