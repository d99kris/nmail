// main.cpp
//
// Copyright (c) 2019-2023 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include <iostream>
#include <memory>

#include "apathy/path.hpp"

#include "addressbook.h"
#include "auth.h"
#include "cacheutil.h"
#include "config.h"
#include "crypto.h"
#include "imapmanager.h"
#include "lockfile.h"
#include "log.h"
#include "loghelp.h"
#include "offlinequeue.h"
#include "sasl.h"
#include "sethelp.h"
#include "smtpmanager.h"
#include "ui.h"
#include "util.h"
#include "version.h"

static bool ValidateConfig(const std::string& p_User, const std::string& p_Imaphost,
                           const uint16_t p_Imapport, const std::string& p_Smtphost,
                           const uint16_t p_Smtpport);
static bool ValidatePass(const std::string& p_Pass, const std::string& p_ErrorPrefix);
static bool ObtainAuthPasswords(const bool p_IsSetup, const std::string& p_User,
                                std::string& p_Pass, std::string& p_SmtpUser, std::string& p_SmtpPass,
                                std::shared_ptr<Config> p_SecretConfig, std::shared_ptr<Config> p_MainConfig);
static bool ObtainCacheEncryptPassword(const bool p_IsSetup, const std::string& p_User,
                                       bool p_SetupAllowCacheEncrypt,
                                       std::string& p_Pass, std::string& p_SmtpUser, std::string& p_SmtpPass,
                                       std::shared_ptr<Config> p_SecretConfig, std::shared_ptr<Config> p_MainConfig);
static bool ReportConfigError(const std::string& p_Param);
static void ShowHelp();
static void ShowVersion();
static void SetupPromptUserDetails(std::shared_ptr<Config> p_Config);
static void SetupGmail(std::shared_ptr<Config> p_Config);
static void SetupGmailCommon(std::shared_ptr<Config> p_Config);
static void SetupGmailOAuth2(std::shared_ptr<Config> p_Config);
static void SetupICloud(std::shared_ptr<Config> p_Config);
static void SetupOutlook(std::shared_ptr<Config> p_Config);
static void SetupOutlookCommon(std::shared_ptr<Config> p_Config);
static void SetupOutlookOAuth2(std::shared_ptr<Config> p_Config);
static void LogSystemInfo();
static bool ChangePasswords(std::shared_ptr<Config> p_MainConfig,
                            std::shared_ptr<Config> p_SecretConfig);
static bool ChangeCachePasswords(std::shared_ptr<Config> p_MainConfig,
                                 const std::string& p_OldPass, const std::string& p_NewPass);
static void KeyDump();

int main(int argc, char* argv[])
{
  // Defaults
  umask(S_IRWXG | S_IRWXO);
  Util::SetApplicationDir(std::string(getenv("HOME")) + std::string("/.nmail"));
  Log::SetVerboseLevel(Log::INFO_LEVEL);
  bool online = true;
  bool changePass = false;
  bool setupAllowCacheEncrypt = false;
  std::string setup;
  std::string exportDir;

  // Argument handling
  std::vector<std::string> args(argv + 1, argv + argc);
  for (auto it = args.begin(); it != args.end(); ++it)
  {
    if ((*it == "-c") || (*it == "--cache-encrypt"))
    {
      setupAllowCacheEncrypt = true;
    }
    else if (((*it == "-d") || (*it == "--confdir")) && (std::distance(it + 1, args.end()) > 0))
    {
      ++it;
      Util::SetApplicationDir(*it);
    }
    else if ((*it == "-e") || (*it == "--verbose"))
    {
      Log::SetVerboseLevel(Log::DEBUG_LEVEL);
    }
    else if ((*it == "-ee") || (*it == "--extra-verbose"))
    {
      Log::SetVerboseLevel(Log::TRACE_LEVEL);
    }
    else if ((*it == "-h") || (*it == "--help"))
    {
      ShowHelp();
      return 0;
    }
    else if ((*it == "-k") || (*it == "--keydump"))
    {
      KeyDump();
      return 0;
    }
    else if ((*it == "-o") || (*it == "--offline"))
    {
      online = false;
    }
    else if ((*it == "-p") || (*it == "--pass"))
    {
      changePass = true;
    }
    else if (((*it == "-s") || (*it == "--setup")) && (std::distance(it + 1, args.end()) > 0))
    {
      ++it;
      setup = *it;
    }
    else if ((*it == "-v") || (*it == "--version"))
    {
      ShowVersion();
      return 0;
    }
    else if (((*it == "-x") || (*it == "--export")) && (std::distance(it + 1, args.end()) > 0))
    {
      ++it;
      exportDir = *it;
    }
    else
    {
      ShowHelp();
      return 1;
    }
  }

  if (!apathy::Path(Util::GetApplicationDir()).exists())
  {
    apathy::Path::makedirs(Util::GetApplicationDir());
  }

  ScopedDirLock dirLock(Util::GetApplicationDir());
  if (!dirLock.IsLocked())
  {
    std::cerr <<
      "error: unable to acquire lock for " << Util::GetApplicationDir() << "\n" <<
      "       only one nmail session per account/confdir is supported.\n";
    return 1;
  }

  const std::string& logPath = Util::GetApplicationDir() + std::string("log.txt");
  Log::SetPath(logPath);

  THREAD_REGISTER();
  Util::RegisterSignalHandlers();

  const std::string appVersion = Version::GetUiAppVersion();
  LOG_INFO("starting %s", appVersion.c_str());

  Util::InitTempDir();
  CacheUtil::InitCacheDir();

  const std::map<std::string, std::string> defaultMainConfig =
  {
    { "name", "" },
    { "address", "" },
    { "user", "" },
    { "imap_host", "" },
    { "imap_port", "993" },
    { "smtp_host", "" },
    { "smtp_port", "587" },
    { "smtp_user", "" },
    { "save_pass", "0" },
    { "inbox", "INBOX" },
    { "trash", "" },
    { "drafts", "" },
    { "sent", "" },
    { "addressbook_encrypt", "0" },
    { "client_store_sent", "0" },
    { "cache_encrypt", "0" },
    { "cache_index_encrypt", "0" },
    { "html_to_text_cmd", "" },
    { "text_to_html_cmd", "" },
    { "parts_viewer_cmd", "" },
    { "html_viewer_cmd", "" },
    { "html_preview_cmd", "" },
    { "msg_viewer_cmd", "" },
    { "prefetch_level", "2" },
    { "prefetch_all_headers", "1" },
    { "verbose_logging", "0" },
    { "pager_cmd", "" },
    { "editor_cmd", "" },
    { "spell_cmd", "" },
    { "folders_exclude", "" },
    { "server_timestamps", "0" },
    { "network_timeout", "30" },
    { "queue_encrypt", "1" },
    { "auth", "pass" },
    { "auth_encrypt", "1" },
    { "send_ip", "1" },
    { "file_picker_cmd", "" },
    { "downloads_dir", "" },
    { "idle_timeout", "29" },
  };
  const std::string mainConfigPath(Util::GetApplicationDir() + std::string("main.conf"));
  std::shared_ptr<Config> mainConfig = std::make_shared<Config>(mainConfigPath, defaultMainConfig);

  const std::string secretConfigPath(Util::GetApplicationDir() + std::string("secret.conf"));

  const bool isSetup = !setup.empty();
  if (isSetup)
  {
    if ((setup != "gmail") && (setup != "gmail-oauth2") && (setup != "icloud") && (setup != "outlook") && (setup != "outlook-oauth2"))
    {
      std::cerr << "error: unsupported email service \"" << setup << "\".\n\n";
      ShowHelp();
      return 1;
    }

    mainConfig = std::make_shared<Config>(mainConfigPath, defaultMainConfig);

    if (setup == "gmail")
    {
      SetupGmail(mainConfig);
    }
    else if (setup == "gmail-oauth2")
    {
      SetupGmailOAuth2(mainConfig);
    }
    else if (setup == "icloud")
    {
      SetupICloud(mainConfig);
    }
    else if (setup == "outlook")
    {
      SetupOutlook(mainConfig);
    }
    else if (setup == "outlook-oauth2")
    {
      SetupOutlookOAuth2(mainConfig);
    }

    remove(mainConfigPath.c_str());
    remove(secretConfigPath.c_str());
    Util::RmDir(Util::GetApplicationDir() + std::string("cache"));
    CacheUtil::InitCacheDir();

    mainConfig->Save();
  }

  // Read secret config
  const std::map<std::string, std::string> defaultSecretConfig;
  std::shared_ptr<Config> secretConfig = std::make_shared<Config>(secretConfigPath, defaultSecretConfig);

  // Read main config
  const std::string& name = mainConfig->Get("name");
  const std::string& address = mainConfig->Get("address");
  const std::string& user = mainConfig->Get("user");
  const std::string& imapHost = mainConfig->Get("imap_host");
  const std::string& smtpHost = mainConfig->Get("smtp_host");
  std::string smtpUser = mainConfig->Get("smtp_user");
  const std::string& inbox = mainConfig->Get("inbox");
  std::string trash = mainConfig->Get("trash");
  std::string drafts = mainConfig->Get("drafts");
  std::string sent = mainConfig->Get("sent");
  const bool clientStoreSent = (mainConfig->Get("client_store_sent") == "1");
  Util::SetHtmlToTextConvertCmd(mainConfig->Get("html_to_text_cmd"));
  Util::SetTextToHtmlConvertCmd(mainConfig->Get("text_to_html_cmd"));
  Util::SetPartsViewerCmd(mainConfig->Get("parts_viewer_cmd"));
  Util::SetHtmlViewerCmd(mainConfig->Get("html_viewer_cmd"));
  Util::SetHtmlPreviewCmd(mainConfig->Get("html_preview_cmd"));
  Util::SetMsgViewerCmd(mainConfig->Get("msg_viewer_cmd"));
  Util::SetPagerCmd(mainConfig->Get("pager_cmd"));
  Util::SetEditorCmd(mainConfig->Get("editor_cmd"));
  Util::SetSpellCmd(mainConfig->Get("spell_cmd"));
  std::set<std::string> foldersExclude = ToSet(Util::SplitQuoted(mainConfig->Get("folders_exclude"), true));
  Util::SetUseServerTimestamps(mainConfig->Get("server_timestamps") == "1");
  const std::string auth = mainConfig->Get("auth");
  const bool prefetchAllHeaders = (mainConfig->Get("prefetch_all_headers") == "1");
  Util::SetSendIp(mainConfig->Get("send_ip") == "1");
  Util::SetFilePickerCmd(mainConfig->Get("file_picker_cmd"));
  Util::SetDownloadsDir(mainConfig->Get("downloads_dir"));

  // Set logging verbosity level based on config, if not specified with command line arguments
  if (Log::GetVerboseLevel() == Log::INFO_LEVEL)
  {
    if (mainConfig->Get("verbose_logging") == "1")
    {
      Log::SetVerboseLevel(Log::DEBUG_LEVEL);
    }
    else if (mainConfig->Get("verbose_logging") == "2")
    {
      Log::SetVerboseLevel(Log::TRACE_LEVEL);
    }
  }

  // Crypto init
  Crypto::Init();

  // Log system info
  if (Log::GetDebugEnabled())
  {
    LogSystemInfo();
  }

  // Log config
  mainConfig->LogParamsExcept(std::set<std::string>({ "name", "address", "user", "smtp_user" }));

  uint16_t imapPort = 0;
  uint16_t smtpPort = 0;
  uint32_t prefetchLevel = 0;
  uint64_t networkTimeout = 0;
  uint32_t idleTimeout = 29;
  try
  {
    imapPort = std::stoi(mainConfig->Get("imap_port"));
    smtpPort = std::stoi(mainConfig->Get("smtp_port"));
    prefetchLevel = std::stoi(mainConfig->Get("prefetch_level"));
    networkTimeout = std::stoll(mainConfig->Get("network_timeout"));
    idleTimeout = std::stoi(mainConfig->Get("idle_timeout"));
  }
  catch (...)
  {
  }

  if (!ValidateConfig(user, imapHost, imapPort, smtpHost, smtpPort))
  {
    ShowHelp();
    return 1;
  }

  if (changePass)
  {
    return ChangePasswords(mainConfig, secretConfig) ? 0 : 1;
  }

  std::string pass;
  std::string smtpPass;
  if (auth == "pass")
  {
    if (!ObtainAuthPasswords(isSetup, user, pass, smtpUser, smtpPass, secretConfig, mainConfig))
    {
      return 1;
    }
  }
  else
  {
    if (!ObtainCacheEncryptPassword(isSetup, user, setupAllowCacheEncrypt, pass, smtpUser, smtpPass, secretConfig, mainConfig))
    {
      return 1;
    }
  }

  // Read config that may be updated during authentication
  const bool cacheEncrypt = (mainConfig->Get("cache_encrypt") == "1");
  const bool cacheIndexEncrypt = (mainConfig->Get("cache_index_encrypt") == "1");
  const bool addressBookEncrypt = (mainConfig->Get("addressbook_encrypt") == "1");
  const bool queueEncrypt = (mainConfig->Get("queue_encrypt") == "1");
  const bool authEncrypt = (mainConfig->Get("auth_encrypt") == "1");

  // Perform export if requested
  if (!exportDir.empty())
  {
    ImapCache imapCache(cacheEncrypt, pass);
    bool exportRv = imapCache.Export(exportDir);
    std::cout << "Export " << (exportRv ? "success" : "failure") << "\n";
    return exportRv ? 0 : 1;
  }

  Util::InitStdErrRedirect(logPath);

  Util::SetAddressBookEncrypt(addressBookEncrypt);

  Auth::Init(auth, authEncrypt, pass, isSetup);

  Ui ui(inbox, address, name, prefetchLevel, prefetchAllHeaders);

  std::shared_ptr<ImapManager> imapManager =
    std::make_shared<ImapManager>(user, pass, imapHost, imapPort, online,
                                  networkTimeout,
                                  cacheEncrypt, cacheIndexEncrypt,
                                  idleTimeout,
                                  foldersExclude,
                                  std::bind(&Ui::ResponseHandler, std::ref(ui), std::placeholders::_1,
                                            std::placeholders::_2),
                                  std::bind(&Ui::ResultHandler, std::ref(ui), std::placeholders::_1,
                                            std::placeholders::_2),
                                  std::bind(&Ui::StatusHandler, std::ref(ui), std::placeholders::_1),
                                  std::bind(&Ui::SearchHandler, std::ref(ui), std::placeholders::_1,
                                            std::placeholders::_2));

  std::shared_ptr<SmtpManager> smtpManager =
    std::make_shared<SmtpManager>(smtpUser, smtpPass, smtpHost, smtpPort, name, address, online,
                                  networkTimeout,
                                  std::bind(&Ui::SmtpResultHandler, std::ref(ui), std::placeholders::_1),
                                  std::bind(&Ui::StatusHandler, std::ref(ui), std::placeholders::_1));

  OfflineQueue::Init(queueEncrypt, pass);

  ui.SetImapManager(imapManager);
  ui.SetTrashFolder(trash);
  ui.SetDraftsFolder(drafts);
  ui.SetSentFolder(sent);
  ui.SetClientStoreSent(clientStoreSent);
  ui.SetSmtpManager(smtpManager);

  imapManager->Start();
  smtpManager->Start();

  ui.Run();

  ui.ResetSmtpManager();
  ui.ResetImapManager();

  smtpManager.reset();
  imapManager.reset();

  Auth::Cleanup();

  mainConfig->Save();
  mainConfig.reset();

  secretConfig->Save();
  secretConfig.reset();

  OfflineQueue::Cleanup();

  Util::CleanupTempDir();

  Util::CleanupStdErrRedirect();

  LOG_INFO("exiting nmail");

  return 0;
}

static void ShowHelp()
{
  std::cout <<
    "nmail is a terminal-based email client with a user interface similar to\n"
    "alpine / pine, supporting IMAP and SMTP.\n"
    "\n"
    "Usage: nmail [OPTION]\n"
    "\n"
    "Options:\n"
    "   -c, --cache-encrypt     prompt for cache encryption during oauth2 setup\n"
    "   -d, --confdir <DIR>     use a different directory than ~/.nmail\n"
    "   -e, --verbose           enable verbose logging\n"
    "   -ee, --extra-verbose    enable extra verbose logging\n"
    "   -h, --help              display this help and exit\n"
    "   -k, --keydump           key code dump mode\n"
    "   -o, --offline           run in offline mode\n"
    "   -p, --pass              change password\n"
    "   -s, --setup <SERVICE>   setup wizard for specified service, supported\n"
    "                           services: gmail, gmail-oauth2, icloud, outlook,\n"
    "                           outlook-oauth2\n"
    "   -v, --version           output version information and exit\n"
    "   -x, --export <DIR>      export cache to specified dir in Maildir format\n"
    "\n"
    "Examples:\n"
    "   nmail -s gmail          setup nmail for a gmail account\n"
    "\n"
    "Files:\n"
    "   ~/.nmail/auth.conf      configures custom oauth2 client id and secret\n"
    "   ~/.nmail/main.conf      configures mail account and general settings\n"
    "   ~/.nmail/ui.conf        customizes UI settings\n"
    "   ~/.nmail/secret.conf    stores saved passwords\n"
    "\n"
    "Report bugs at https://github.com/d99kris/nmail\n"
    "\n";
}

static void ShowVersion()
{
  std::cout <<
    Version::GetUiAppVersion() << "\n"
    "\n"
    "Copyright (c) 2019-2023 Kristofer Berggren\n"
    "\n"
    "nmail is distributed under the MIT license.\n"
    "\n"
    "Written by Kristofer Berggren.\n";
}

static void SetupPromptUserDetails(std::shared_ptr<Config> p_Config)
{
  std::string email;
  std::cout << "Email: ";
  std::getline(std::cin, email);
  std::cout << "Name: ";
  std::string name;
  std::getline(std::cin, name);

  p_Config->Set("name", name);
  p_Config->Set("address", email);
  p_Config->Set("user", email);
}

static void SetupGmail(std::shared_ptr<Config> p_Config)
{
  SetupPromptUserDetails(p_Config);

  SetupGmailCommon(p_Config);
}

static void SetupGmailCommon(std::shared_ptr<Config> p_Config)
{
  p_Config->Set("imap_host", "imap.gmail.com");
  p_Config->Set("imap_port", "993");
  p_Config->Set("smtp_host", "smtp.gmail.com");
  p_Config->Set("smtp_port", "465");
  p_Config->Set("inbox", "INBOX");
  p_Config->Set("trash", "[Gmail]/Trash");
  p_Config->Set("drafts", "[Gmail]/Drafts");
  p_Config->Set("sent", "[Gmail]/Sent Mail");
  p_Config->Set("folders_exclude", "\"[Gmail]/All Mail\",\"[Gmail]/Important\",\"[Gmail]/Starred\"");
}

static void SetupGmailOAuth2(std::shared_ptr<Config> p_Config)
{
  std::string auth = "gmail-oauth2";
  if (!Auth::GenerateToken(auth))
  {
    std::cout << auth << " setup failed, exiting.\n";
    exit(1);
  }

  std::string name = Auth::GetName();
  std::string email = Auth::GetEmail();
  p_Config->Set("name", name);
  p_Config->Set("address", email);
  p_Config->Set("user", email);
  p_Config->Set("auth", auth);

  SetupGmailCommon(p_Config);
}

static void SetupICloud(std::shared_ptr<Config> p_Config)
{
  SetupPromptUserDetails(p_Config);

  p_Config->Set("imap_host", "imap.mail.me.com");
  p_Config->Set("smtp_host", "smtp.mail.me.com");
  p_Config->Set("inbox", "INBOX");
  p_Config->Set("trash", "Deleted Messages");
  p_Config->Set("drafts", "Drafts");
  p_Config->Set("sent", "Sent Messages");
  p_Config->Set("client_store_sent", "1");
}

static void SetupOutlook(std::shared_ptr<Config> p_Config)
{
  SetupPromptUserDetails(p_Config);

  SetupOutlookCommon(p_Config);
  p_Config->Set("imap_host", "imap-mail.outlook.com");
  p_Config->Set("smtp_host", "smtp-mail.outlook.com");
}

static void SetupOutlookCommon(std::shared_ptr<Config> p_Config)
{
  p_Config->Set("inbox", "Inbox");
  p_Config->Set("trash", "Deleted");
  p_Config->Set("drafts", "Drafts");
  p_Config->Set("sent", "Sent");
}

static void SetupOutlookOAuth2(std::shared_ptr<Config> p_Config)
{
  std::string auth = "outlook-oauth2";
  if (!Auth::GenerateToken(auth))
  {
    std::cout << auth << " setup failed, exiting.\n";
    exit(1);
  }

  std::string name = Auth::GetName();
  std::string email = Auth::GetEmail();
  p_Config->Set("name", name);
  p_Config->Set("address", email);
  p_Config->Set("user", email);
  p_Config->Set("auth", auth);

  SetupOutlookCommon(p_Config);
  p_Config->Set("imap_host", "outlook.office365.com");
  p_Config->Set("smtp_host", "outlook.office365.com");
}

static bool ObtainAuthPasswords(const bool p_IsSetup, const std::string& p_User,
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
    std::cout << (p_SmtpUser.empty() ? "Password: " : "IMAP Password: ");
    p_Pass = Util::GetPass();
    encPass = Util::ToHex(Crypto::AESEncrypt(p_Pass, p_User));
  }
  else
  {
    p_Pass = Crypto::AESDecrypt(Util::FromHex(encPass), p_User);
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
      std::cout << "SMTP Password: ";
      p_SmtpPass = Util::GetPass();
      encSmtpPass = Util::ToHex(Crypto::AESEncrypt(p_SmtpPass, p_SmtpUser));
    }
    else
    {
      p_SmtpPass = Crypto::AESDecrypt(Util::FromHex(encSmtpPass), p_SmtpUser);
    }
  }

  if (!ValidatePass(p_SmtpPass, "SMTP "))
  {
    return false;
  }

  if (p_IsSetup)
  {
    std::cout << "Save password (y/n): ";
    std::string savepass;
    std::getline(std::cin, savepass);
    const bool isSavePass = (savepass == "y");
    p_MainConfig->Set("save_pass", isSavePass ? "1" : "0");
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

static bool ObtainCacheEncryptPassword(const bool p_IsSetup, const std::string& p_User,
                                       bool p_SetupAllowCacheEncrypt,
                                       std::string& p_Pass, std::string& p_SmtpUser, std::string& p_SmtpPass,
                                       std::shared_ptr<Config> p_SecretConfig, std::shared_ptr<Config> p_MainConfig)
{
  if (p_IsSetup)
  {
    if (p_SetupAllowCacheEncrypt)
    {
      std::cout << "Cache Encryption Password (optional): ";
      p_Pass = Util::GetPass();
    }

    if (p_Pass.empty())
    {
      // if no pass specified during setup, disable encryption
      p_MainConfig->Set("cache_encrypt", "0");
      p_MainConfig->Set("cache_index_encrypt", "0");
      p_MainConfig->Set("addressbook_encrypt", "0");
      p_MainConfig->Set("queue_encrypt", "0");
      p_MainConfig->Set("auth_encrypt", "0");

      p_MainConfig->Set("save_pass", "0");
    }
    else
    {
      // if pass specified, prompt user whether to store it
      std::cout << "Save password (y/n): ";
      std::string savepass;
      std::getline(std::cin, savepass);
      const bool isSavePass = (savepass == "y");
      p_MainConfig->Set("save_pass", isSavePass ? "1" : "0");

      if (isSavePass)
      {
        std::string encPass = Util::ToHex(Crypto::AESEncrypt(p_Pass, p_User));
        p_SecretConfig->Set("pass", encPass);
      }
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
        p_Pass = Util::GetPass();
        encPass = Util::ToHex(Crypto::AESEncrypt(p_Pass, p_User));
      }
      else
      {
        p_Pass = Crypto::AESDecrypt(Util::FromHex(encPass), p_User);
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

bool ValidateConfig(const std::string& p_User, const std::string& p_Imaphost,
                    const uint16_t p_Imapport, const std::string& p_Smtphost,
                    const uint16_t p_Smtpport)
{
  if (p_User.empty()) return ReportConfigError("user");
  if (p_Imaphost.empty()) return ReportConfigError("imaphost");
  if (p_Imapport == 0) return ReportConfigError("imapport");
  if (p_Smtphost.empty()) return ReportConfigError("smtphost");
  if (p_Smtpport == 0) return ReportConfigError("smtpport");

  return true;
}

bool ValidatePass(const std::string& p_Pass, const std::string& p_ErrorPrefix)
{
  if (p_Pass.empty())
  {
    std::cerr << "error: " << p_ErrorPrefix << "pass not specified.\n\n";
    return false;
  }

  return true;
}

bool ReportConfigError(const std::string& p_Param)
{
  const std::string configPath(Util::GetApplicationDir() + std::string("main.conf"));
  std::cerr << "error: " << p_Param << " not specified in config file (" << configPath
            << ").\n\n";
  return false;
}

void LogSystemInfo()
{
  const std::string buildOs = Version::GetBuildOs();
  LOG_DEBUG("build os:  %s", buildOs.c_str());

  const std::string compiler = Version::GetCompiler();
  LOG_DEBUG("compiler:  %s", compiler.c_str());

  const std::string systemOs = Util::GetSystemOs();
  LOG_DEBUG("system os: %s", systemOs.c_str());

  const std::string unameStr = Util::GetUname();
  if (!unameStr.empty())
  {
    LOG_DEBUG("uname:   ");
    LOG_DUMP(unameStr.c_str());
  }

  const std::string libetpanVersion = Util::GetLibetpanVersion();
  LOG_DEBUG("libetpan:  %s", libetpanVersion.c_str());

  const std::string saslMechs = Sasl::GetMechanismsStr();
  LOG_DEBUG("saslmechs: %s", saslMechs.c_str());

  const std::string libxapianVersion = SearchEngine::GetXapianVersion();
  LOG_DEBUG("libxapian: %s", libxapianVersion.c_str());

  const std::string openSSLVersion = Crypto::GetVersion();
  LOG_DEBUG("openssl:   %s", openSSLVersion.c_str());

  const std::string sqliteVersion = Util::GetSQLiteVersion();
  LOG_DEBUG("sqlite:    %s", sqliteVersion.c_str());

  const std::string selfPath = Util::GetSelfPath();
  if (!selfPath.empty())
  {
    const std::string linkedLibs = Util::GetLinkedLibs(selfPath);
    if (!linkedLibs.empty())
    {
      LOG_DEBUG("libs:    ");
      LOG_DUMP(linkedLibs.c_str());
    }
  }
}

bool ChangePasswords(std::shared_ptr<Config> p_MainConfig, std::shared_ptr<Config> p_SecretConfig)
{
  std::string user = p_MainConfig->Get("user");
  std::string smtpUser = p_MainConfig->Get("smtp_user");

  std::string oldPass;
  if (p_SecretConfig->Exist("pass"))
  {
    std::string oldEncPass = p_SecretConfig->Get("pass");
    oldPass = Crypto::AESDecrypt(Util::FromHex(oldEncPass), user);
  }
  else
  {
    std::cout << std::string("Old ") + (smtpUser.empty() ? "Password: " : "IMAP Password: ");
    oldPass = Util::GetPass();
  }

  std::cout << std::string("New ") + (smtpUser.empty() ? "Password: " : "IMAP Password: ");
  std::string newPass = Util::GetPass();

  std::string newSmtpPass;
  if (!smtpUser.empty())
  {
    std::cout << "SMTP Password: ";
    newSmtpPass = Util::GetPass();
  }

  if (ChangeCachePasswords(p_MainConfig, oldPass, newPass))
  {
    std::string encPass = Util::ToHex(Crypto::AESEncrypt(newPass, user));
    p_SecretConfig->Set("pass", encPass);

    if (!smtpUser.empty())
    {
      std::string encSmtpPass = Util::ToHex(Crypto::AESEncrypt(newSmtpPass, smtpUser));
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

bool ChangeCachePasswords(std::shared_ptr<Config> p_MainConfig,
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

static void KeyDump()
{
  setlocale(LC_ALL, "");
  initscr();
  noecho();
  cbreak();
  raw();
  keypad(stdscr, TRUE);
  curs_set(0);
  timeout(0);

  printw("key code dump mode - press ctrl-c or 'q' to exit\n");
  refresh();

  bool running = true;
  while (running)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    int maxfd = STDIN_FILENO;
    struct timeval tv = {1, 0};
    select(maxfd + 1, &fds, NULL, NULL, &tv);
    if (FD_ISSET(STDIN_FILENO, &fds))
    {
      int y = 0;
      int x = 0;
      int maxy = 0;
      int maxx = 0;
      getyx(stdscr, y, x);
      getmaxyx(stdscr, maxy, maxx);
      if (y == (maxy - 1))
      {
        clear();
        refresh();
      }

      int count = 0;
      wint_t key = 0;
      wint_t keyOk = 0;
      while (get_wch(&key) != ERR)
      {
        keyOk = key;
        ++count;
        printw("\\%o", key);

        if ((key == 3) || (key == 'q'))
        {
          running = false;
          break;
        }
      }

      if ((keyOk != 0) && (count == 1))
      {
        std::string keyName = Util::GetKeyName(keyOk);
        if (!keyName.empty())
        {
          printw(" %s", keyName.c_str());
        }
      }

      printw("\n");
      refresh();
    }
  }

  wclear(stdscr);
  endwin();
}
