// passutil.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "passutil.h"

#include <iostream>

#include "addressbook.h"
#include "auth.h"
#include "crypto.h"
#include "imapcache.h"
#include "imapindex.h"
#include "mainconfig.h"
#include "offlinequeue.h"
#include "setupwizard.h"
#include "strutil.h"
#include "sysutil.h"

bool PassUtil::ObtainAuthPasswords(const bool p_IsSetup, const std::string& p_User,
                                   std::string& p_Pass, std::string& p_SmtpUser, std::string& p_SmtpPass,
                                   std::shared_ptr<Config> p_SecretConfig)
{
  // Read secret config
  std::string encPass;
  if (p_SecretConfig->Exist("pass"))
  {
    encPass = p_SecretConfig->Get("pass");
  }

  std::string encSmtpPass;
  if (p_SecretConfig->Exist("smtp_pass"))
  {
    encSmtpPass = p_SecretConfig->Get("smtp_pass");
  }

  if (encPass.empty())
  {
    const std::string label = p_SmtpUser.empty() ? "Password" : "IMAP Password";
    if (p_IsSetup)
    {
      if (!SetupWizard::PromptPassword(label, p_Pass)) return false;
    }
    else
    {
      std::cout << label << ": " << std::flush;
      p_Pass = SysUtil::GetPass();
    }

    encPass = StrUtil::ToHex(Crypto::AESEncrypt(p_Pass, p_User));
  }
  else
  {
    p_Pass = Crypto::AESDecrypt(StrUtil::FromHex(encPass), p_User);
  }

  if (!ValidatePass(p_Pass, p_SmtpUser.empty() ? "" : "IMAP "))
  {
    return false;
  }

  if (p_SmtpUser.empty())
  {
    p_SmtpUser = p_User;
    p_SmtpPass = p_Pass;
  }
  else
  {
    if (encSmtpPass.empty())
    {
      std::cout << (p_IsSetup ? SetupWizard::Label("SMTP Password") : "SMTP Password: ") << std::flush;
      p_SmtpPass = SysUtil::GetPass();
      encSmtpPass = StrUtil::ToHex(Crypto::AESEncrypt(p_SmtpPass, p_SmtpUser));
    }
    else
    {
      p_SmtpPass = Crypto::AESDecrypt(StrUtil::FromHex(encSmtpPass), p_SmtpUser);
    }
  }

  if (!ValidatePass(p_SmtpPass, "SMTP "))
  {
    return false;
  }

  if (p_IsSetup)
  {
    std::cout << "\n";
    bool isSavePass = true;
    if (!SetupWizard::PromptYesNo("Save password", isSavePass)) return false;

    MainConfig::SetStr("save_pass", isSavePass ? "1" : "0");
    std::cout << "\n";
  }

  const bool isSavePass = MainConfig::GetBool("save_pass");
  if (isSavePass)
  {
    if (!encPass.empty())
    {
      p_SecretConfig->Set("pass", encPass);
    }

    if (!encSmtpPass.empty())
    {
      p_SecretConfig->Set("smtp_pass", encSmtpPass);
    }
  }

  return true;
}

bool PassUtil::ObtainCacheEncryptPassword(const bool p_IsSetup, const std::string& p_User,
                                          bool p_SetupAllowCacheEncrypt,
                                          std::string& p_Pass, std::string& p_SmtpUser, std::string& p_SmtpPass,
                                          std::shared_ptr<Config> p_SecretConfig)
{
  if (p_IsSetup)
  {
    if (p_SetupAllowCacheEncrypt)
    {
      std::cout << SetupWizard::Label("Cache Encryption Password (optional)") << std::flush;
      p_Pass = SysUtil::GetPass();
    }

    if (p_Pass.empty())
    {
      // if no pass specified during setup, disable encryption
      MainConfig::SetStr("cache_encrypt", "0");
      MainConfig::SetStr("cache_index_encrypt", "0");
      MainConfig::SetStr("addressbook_encrypt", "0");
      MainConfig::SetStr("queue_encrypt", "0");
      MainConfig::SetStr("auth_encrypt", "0");
    }
    else
    {
      // if pass specified, prompt user whether to store it
      std::cout << "\n";
      bool isSavePass = true;
      if (!SetupWizard::PromptYesNo("Save password", isSavePass)) return false;

      MainConfig::SetStr("save_pass", isSavePass ? "1" : "0");

      if (isSavePass)
      {
        std::string encPass = StrUtil::ToHex(Crypto::AESEncrypt(p_Pass, p_User));
        p_SecretConfig->Set("pass", encPass);
      }
    }

    if (p_SetupAllowCacheEncrypt)
    {
      std::cout << "\n";
    }

    p_SecretConfig->Save();
    MainConfig::Save();
  }
  else
  {
    const bool cacheEncrypt = MainConfig::GetBool("cache_encrypt");
    const bool cacheIndexEncrypt = MainConfig::GetBool("cache_index_encrypt");
    const bool addressBookEncrypt = MainConfig::GetBool("addressbook_encrypt");
    const bool queueEncrypt = MainConfig::GetBool("queue_encrypt");
    const bool authEncrypt = MainConfig::GetBool("auth_encrypt");

    if (!cacheEncrypt && !cacheIndexEncrypt && !addressBookEncrypt && !queueEncrypt && !authEncrypt)
    {
      p_Pass = "";
    }
    else
    {
      std::string encPass;
      if (p_SecretConfig->Exist("pass"))
      {
        encPass = p_SecretConfig->Get("pass");
      }

      if (encPass.empty())
      {
        std::cout << "Cache Encryption Password: ";
        p_Pass = SysUtil::GetPass();
        encPass = StrUtil::ToHex(Crypto::AESEncrypt(p_Pass, p_User));
      }
      else
      {
        p_Pass = Crypto::AESDecrypt(StrUtil::FromHex(encPass), p_User);
      }

      if (!ValidatePass(p_Pass, "Cache Encryption "))
      {
        return false;
      }
    }
  }

  if (p_SmtpUser.empty())
  {
    p_SmtpUser = p_User;
  }

  p_SmtpPass = p_Pass;

  return true;
}

bool PassUtil::ValidatePass(const std::string& p_Pass, const std::string& p_ErrorPrefix)
{
  if (p_Pass.empty())
  {
    std::cerr << "error: " << p_ErrorPrefix << "pass not specified.\n\n";
    return false;
  }

  return true;
}

bool PassUtil::ChangePasswords(std::shared_ptr<Config> p_SecretConfig)
{
  std::string user = MainConfig::GetStr("user");
  std::string smtpUser = MainConfig::GetStr("smtp_user");

  std::string oldPass;
  if (p_SecretConfig->Exist("pass"))
  {
    std::string oldEncPass = p_SecretConfig->Get("pass");
    oldPass = Crypto::AESDecrypt(StrUtil::FromHex(oldEncPass), user);
  }
  else
  {
    std::cout << std::string("Old ") + (smtpUser.empty() ? "Password: " : "IMAP Password: ");
    oldPass = SysUtil::GetPass();
  }

  std::cout << std::string("New ") + (smtpUser.empty() ? "Password: " : "IMAP Password: ");
  std::string newPass = SysUtil::GetPass();

  std::string newSmtpPass;
  if (!smtpUser.empty())
  {
    std::cout << "SMTP Password: ";
    newSmtpPass = SysUtil::GetPass();
  }

  if (ChangeCachePasswords(oldPass, newPass))
  {
    std::string encPass = StrUtil::ToHex(Crypto::AESEncrypt(newPass, user));
    p_SecretConfig->Set("pass", encPass);

    if (!smtpUser.empty())
    {
      std::string encSmtpPass = StrUtil::ToHex(Crypto::AESEncrypt(newSmtpPass, smtpUser));
      p_SecretConfig->Set("smtp_pass", encSmtpPass);
    }

    p_SecretConfig->Save();

    std::cout << "Changing password complete.\n";
    return true;
  }
  else
  {
    std::cout << "Changing cache password failed, exiting.\n";
    return false;
  }
}

bool PassUtil::ChangeCachePasswords(const std::string& p_OldPass, const std::string& p_NewPass)
{
  const bool cacheEncrypt = MainConfig::GetBool("cache_encrypt");
  if (!ImapCache::ChangePass(cacheEncrypt, p_OldPass, p_NewPass)) return false;

  const bool cacheIndexEncrypt = MainConfig::GetBool("cache_index_encrypt");
  if (!ImapIndex::ChangePass(cacheIndexEncrypt, p_OldPass, p_NewPass)) return false;

  const bool addressBookEncrypt = MainConfig::GetBool("addressbook_encrypt");
  if (!AddressBook::ChangePass(addressBookEncrypt, p_OldPass, p_NewPass)) return false;

  const bool queueEncrypt = MainConfig::GetBool("queue_encrypt");
  if (!OfflineQueue::ChangePass(queueEncrypt, p_OldPass, p_NewPass)) return false;

  const bool authEncrypt = MainConfig::GetBool("auth_encrypt");
  if (!Auth::ChangePass(authEncrypt, p_OldPass, p_NewPass)) return false;

  return true;
}
