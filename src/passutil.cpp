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
#include "offlinequeue.h"
#include "setupwizard.h"
#include "strutil.h"
#include "sysutil.h"

bool PassUtil::ObtainAuthPasswords(const bool p_IsSetup, const std::string& p_User,
                                   std::string& p_Pass, std::string& p_SmtpUser, std::string& p_SmtpPass,
                                   std::shared_ptr<Config> p_SecretConfig, std::shared_ptr<Config> p_MainConfig)
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
      if (!SetupWizard::PromptPassword(label, p_MainConfig, p_Pass)) return false;
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

    p_MainConfig->Set("save_pass", isSavePass ? "1" : "0");
    std::cout << "\n";
  }

  const bool isSavePass = (p_MainConfig->Get("save_pass") == "1");
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
                                          std::shared_ptr<Config> p_SecretConfig, std::shared_ptr<Config> p_MainConfig)
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
      p_MainConfig->Set("cache_encrypt", "0");
      p_MainConfig->Set("cache_index_encrypt", "0");
      p_MainConfig->Set("addressbook_encrypt", "0");
      p_MainConfig->Set("queue_encrypt", "0");
      p_MainConfig->Set("auth_encrypt", "0");
    }
    else
    {
      // if pass specified, prompt user whether to store it
      std::cout << "\n";
      bool isSavePass = true;
      if (!SetupWizard::PromptYesNo("Save password", isSavePass)) return false;

      p_MainConfig->Set("save_pass", isSavePass ? "1" : "0");

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
    p_MainConfig->Save();
  }
  else
  {
    const bool cacheEncrypt = (p_MainConfig->Get("cache_encrypt") == "1");
    const bool cacheIndexEncrypt = (p_MainConfig->Get("cache_index_encrypt") == "1");
    const bool addressBookEncrypt = (p_MainConfig->Get("addressbook_encrypt") == "1");
    const bool queueEncrypt = (p_MainConfig->Get("queue_encrypt") == "1");
    const bool authEncrypt = (p_MainConfig->Get("auth_encrypt") == "1");

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

bool PassUtil::ChangePasswords(std::shared_ptr<Config> p_MainConfig, std::shared_ptr<Config> p_SecretConfig)
{
  std::string user = p_MainConfig->Get("user");
  std::string smtpUser = p_MainConfig->Get("smtp_user");

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

  if (ChangeCachePasswords(p_MainConfig, oldPass, newPass))
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

bool PassUtil::ChangeCachePasswords(std::shared_ptr<Config> p_MainConfig,
                                    const std::string& p_OldPass, const std::string& p_NewPass)
{
  const bool cacheEncrypt = (p_MainConfig->Get("cache_encrypt") == "1");
  if (!ImapCache::ChangePass(cacheEncrypt, p_OldPass, p_NewPass)) return false;

  const bool cacheIndexEncrypt = (p_MainConfig->Get("cache_index_encrypt") == "1");
  if (!ImapIndex::ChangePass(cacheIndexEncrypt, p_OldPass, p_NewPass)) return false;

  const bool addressBookEncrypt = (p_MainConfig->Get("addressbook_encrypt") == "1");
  if (!AddressBook::ChangePass(addressBookEncrypt, p_OldPass, p_NewPass)) return false;

  const bool queueEncrypt = (p_MainConfig->Get("queue_encrypt") == "1");
  if (!OfflineQueue::ChangePass(queueEncrypt, p_OldPass, p_NewPass)) return false;

  const bool authEncrypt = (p_MainConfig->Get("auth_encrypt") == "1");
  if (!Auth::ChangePass(authEncrypt, p_OldPass, p_NewPass)) return false;

  return true;
}
