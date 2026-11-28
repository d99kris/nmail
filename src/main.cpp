// main.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include <cstdlib>
#include <iostream>
#include <memory>

#include <signal.h>
#include <unistd.h>

#include "apathy/path.hpp"

#include "apputil.h"
#include "auth.h"
#include "cacheutil.h"
#include "config.h"
#include "crypto.h"
#include "debuginfo.h"
#include "extutil.h"
#include "fileutil.h"
#include "imapmanager.h"
#include "lockfile.h"
#include "log.h"
#include "loghelp.h"
#include "offlinequeue.h"
#include "passutil.h"
#include "sasl.h"
#include "sethelp.h"
#include "setupwizard.h"
#include "smtpmanager.h"
#include "strutil.h"
#include "sysutil.h"
#include "theme.h"
#include "ui.h"
#include "uikeydump.h"
#include "upgrade.h"
#include "version.h"

static bool ValidateConfig(const std::string& p_User, const std::string& p_Imaphost,
                           const uint16_t p_Imapport, const std::string& p_Smtphost,
                           const uint16_t p_Smtpport);
static bool ReportConfigError(const std::string& p_Param);
static void ShowHelp();
static void ShowVersion();
static void LogSystemInfo();
static std::string GetDisplayPath(const std::string& p_Path);

int main(int argc, char* argv[])
{
  // Defaults
  const mode_t origUmask = umask(S_IRWXG | S_IRWXO);
  AppUtil::SetApplicationDir(AppUtil::GetDefaultApplicationDir());
  Log::SetVerboseLevel(Log::INFO_LEVEL);
  bool online = true;
  bool changePass = false;
  bool keyDump = false;
  bool readOnly = false;
  bool setupAllowCacheEncrypt = false;
  bool themeInstall = false;
  bool upgrade = false;
  std::string setup;
  std::string theme;
  std::string upgradeVersion;
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
      AppUtil::SetApplicationDir(*it);
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
      keyDump = true;
    }
    else if ((*it == "-o") || (*it == "--offline"))
    {
      online = false;
    }
    else if ((*it == "-p") || (*it == "--pass"))
    {
      changePass = true;
    }
    else if ((*it == "-ro") || (*it == "--read-only"))
    {
      readOnly = true;
    }
    else if ((*it == "-s") || (*it == "--setup"))
    {
      // optional service name, detect from email address if not specified
      setup = "auto";
      if ((std::distance(it + 1, args.end()) > 0) && ((it + 1)->rfind("-", 0) != 0))
      {
        ++it;
        setup = *it;
      }
    }
    else if ((*it == "-t") || (*it == "--theme"))
    {
      // optional theme name, prompt for selection if not specified
      themeInstall = true;
      if ((std::distance(it + 1, args.end()) > 0) && ((it + 1)->rfind("-", 0) != 0))
      {
        ++it;
        theme = *it;
      }
    }
    else if ((*it == "-u") || (*it == "--upgrade"))
    {
      // optional release version, latest if not specified
      upgrade = true;
      if ((std::distance(it + 1, args.end()) > 0) && ((it + 1)->rfind("-", 0) != 0))
      {
        ++it;
        upgradeVersion = *it;
      }
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

  if (themeInstall)
  {
    return Theme::Install(theme) ? 0 : 1;
  }

  if (upgrade)
  {
    return Upgrade::Run(upgradeVersion, origUmask);
  }

  // start setup wizard on first run
  const std::string mainConfigFile = AppUtil::GetApplicationDir() + std::string("main.conf");
  const bool hasMainConfig = apathy::Path(mainConfigFile).exists();
  if (setup.empty() && !readOnly && !changePass && !keyDump && exportDir.empty() && !hasMainConfig &&
      isatty(STDIN_FILENO))
  {
    std::cout << "No config found in " << GetDisplayPath(AppUtil::GetApplicationDir()) << "\n\n";
    setup = "auto";
  }

  const bool isSetup = !setup.empty();

  std::shared_ptr<ScopedDirLock> dirLock;
  if (!readOnly)
  {
    if (!apathy::Path(AppUtil::GetApplicationDir()).exists())
    {
      apathy::Path::makedirs(AppUtil::GetApplicationDir());
    }

    dirLock.reset(new ScopedDirLock(AppUtil::GetApplicationDir()));
    if (!dirLock->IsLocked())
    {
      readOnly = true;
    }
  }

  if (readOnly && (isSetup || changePass))
  {
    std::cerr <<
      "error: setup and password change require exclusive access to config dir\n"
      "       " << AppUtil::GetApplicationDir() << "\n" <<
      "       ensure no other nmail instance is running, and -ro is not specified.\n";
    return 1;
  }

  // Note: must be set before any Config is constructed, as Config write access depends on it
  AppUtil::SetReadOnly(readOnly);

  const std::string& logPath = AppUtil::GetApplicationDir() + std::string("log.txt");
  Log::SetPath(logPath);

  THREAD_REGISTER();
  AppUtil::InitAppSignalHandlers();

  const std::string appVersion = Version::GetAppName(true /*p_WithVersion*/, true /*p_WithBranch*/);
  LOG_INFO("%s", appVersion.c_str());
  std::string osArch = SysUtil::GetOsArch(false /*p_Verbose*/);
  LOG_INFO("%s", osArch.c_str());
  std::string buildInfo = SysUtil::GetBuildInfo();
  LOG_INFO("%s", buildInfo.c_str());
  std::string compiler = SysUtil::GetCompiler(false /*p_Verbose*/);
  LOG_INFO("%s", compiler.c_str());

  if (readOnly)
  {
    LOG_INFO("read-only mode");
  }

  AppUtil::CheckEnvironment();

  FileUtil::InitTempDir();
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
    { "save_pass", "1" },
    { "idle_inbox", "1" },
    { "inbox", "INBOX" },
    { "trash", "" },
    { "spam", "" },
    { "drafts", "" },
    { "sent", "" },
    { "addressbook_encrypt", "0" },
    { "cache_encrypt", "0" },
    { "cache_index_encrypt", "0" },
    { "client_store_sent", "0" },
    { "coredump_enabled", "0" },
    { "html_to_text_cmd", "" },
    { "text_to_html_cmd", "" },
    { "parts_viewer_cmd", "" },
    { "html_viewer_cmd", "" },
    { "html_preview_cmd", "" },
    { "msg_viewer_cmd", "" },
    { "link_open_cmd", "" },
    { "prefetch_level", "2" },
    { "prefetch_all_headers", "1" },
    { "verbose_logging", "0" },
    { "pager_cmd", "" },
    { "editor_cmd", "" },
    { "spell_cmd", "" },
    { "browser_cmd", "" },
    { "folders_exclude", "" },
    { "server_timestamps", "0" },
    { "network_timeout_sec", "30" },
    { "queue_encrypt", "1" },
    { "auth", "pass" },
    { "auth_encrypt", "1" },
    { "send_ip", "1" },
    { "file_picker_cmd", "" },
    { "downloads_dir", "" },
    { "idle_timeout_min", "29" },
    { "idle_delay_sec", "15" },
    { "sni_enabled", "1" },
    { "logdump_enabled", "0" },
    { "copy_to_trash", "" },
    { "assert_abort", "0" },
  };
  const std::map<std::string, std::string> renamedMainConfig =
  {
    { "idle_timeout", "idle_timeout_min" },
    { "network_timeout", "network_timeout_sec" },
  };
  const std::string mainConfigPath(AppUtil::GetApplicationDir() + std::string("main.conf"));
  std::shared_ptr<Config> mainConfig =
    std::make_shared<Config>(mainConfigPath, defaultMainConfig, false, renamedMainConfig);

  const std::string secretConfigPath(AppUtil::GetApplicationDir() + std::string("secret.conf"));

  if (keyDump)
  {
    LOG_INFO("start keydump");
    UiKeyDump::Run();
    return 0;
  }

  // Init debug info, log last version
  DebugInfo::Init();
  const std::string versionUsed = DebugInfo::GetStr("version_used");
  if (!versionUsed.empty() && (versionUsed != Version::GetAppVersion()))
  {
    LOG_INFO("last version %s", versionUsed.c_str());
  }

  // Read params needed for setup
  ExtUtil::SetBrowserCmd(mainConfig->Get("browser_cmd"));

  if (isSetup)
  {
    LOG_INFO("start setup");
    mainConfig = std::make_shared<Config>(mainConfigPath, defaultMainConfig, false, renamedMainConfig);

    if (!SetupWizard::Run(setup, mainConfig, defaultMainConfig))
    {
      if (!hasMainConfig)
      {
        // remove default config created above, to rerun setup on next start
        remove(mainConfigPath.c_str());
      }

      return 1;
    }

    remove(mainConfigPath.c_str());
    remove(secretConfigPath.c_str());
    FileUtil::RmDir(AppUtil::GetApplicationDir() + std::string("cache"));
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
  std::string spam = mainConfig->Get("spam");
  std::string drafts = mainConfig->Get("drafts");
  std::string sent = mainConfig->Get("sent");
  const bool clientStoreSent = (mainConfig->Get("client_store_sent") == "1");
  const bool idleInbox = (mainConfig->Get("idle_inbox") == "1");
  ExtUtil::SetHtmlToTextConvertCmd(mainConfig->Get("html_to_text_cmd"));
  ExtUtil::SetTextToHtmlConvertCmd(mainConfig->Get("text_to_html_cmd"));
  ExtUtil::SetPartsViewerCmd(mainConfig->Get("parts_viewer_cmd"));
  ExtUtil::SetHtmlViewerCmd(mainConfig->Get("html_viewer_cmd"));
  ExtUtil::SetHtmlPreviewCmd(mainConfig->Get("html_preview_cmd"));
  ExtUtil::SetMsgViewerCmd(mainConfig->Get("msg_viewer_cmd"));
  ExtUtil::SetLinkOpenCmd(mainConfig->Get("link_open_cmd"));
  ExtUtil::SetPagerCmd(mainConfig->Get("pager_cmd"));
  ExtUtil::SetEditorCmd(mainConfig->Get("editor_cmd"));
  ExtUtil::SetSpellCmd(mainConfig->Get("spell_cmd"));
  std::set<std::string> foldersExclude = ToSet(StrUtil::SplitQuoted(mainConfig->Get("folders_exclude"), true));
  AppUtil::SetUseServerTimestamps(mainConfig->Get("server_timestamps") == "1");
  const std::string auth = mainConfig->Get("auth");
  const bool prefetchAllHeaders = (mainConfig->Get("prefetch_all_headers") == "1");
  AppUtil::SetSendIp(mainConfig->Get("send_ip") == "1");
  ExtUtil::SetFilePickerCmd(mainConfig->Get("file_picker_cmd"));
  FileUtil::SetDownloadsDir(mainConfig->Get("downloads_dir"));
  const bool isCoredumpEnabled = (mainConfig->Get("coredump_enabled") == "1");
  const bool sniEnabled = (mainConfig->Get("sni_enabled") == "1");
  const bool isLogdumpEnabled = (mainConfig->Get("logdump_enabled") == "1");
  AppUtil::SetCopyToTrash(mainConfig->Get("copy_to_trash"), mainConfig->Get("imap_host"));
  mainConfig->Set("copy_to_trash", std::to_string(AppUtil::GetCopyToTrash()));
  AppUtil::SetAssertAbort(mainConfig->Get("assert_abort") == "1");

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

  // Init core dump
  if (isCoredumpEnabled)
  {
#ifndef HAS_COREDUMP
    LOG_WARNING("core dump not supported");
#else
    AppUtil::InitCoredump();
#endif
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
  uint32_t idleDelay = 15;
  try
  {
    imapPort = std::stoi(mainConfig->Get("imap_port"));
    smtpPort = std::stoi(mainConfig->Get("smtp_port"));
    prefetchLevel = std::stoi(mainConfig->Get("prefetch_level"));
    networkTimeout = std::stoll(mainConfig->Get("network_timeout_sec"));
    idleTimeout = std::stoi(mainConfig->Get("idle_timeout_min"));
    idleDelay = std::stoi(mainConfig->Get("idle_delay_sec"));
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
    return PassUtil::ChangePasswords(mainConfig, secretConfig) ? 0 : 1;
  }

  std::string pass;
  std::string smtpPass;
  if (auth == "pass")
  {
    if (!PassUtil::ObtainAuthPasswords(isSetup, user, pass, smtpUser, smtpPass, secretConfig, mainConfig))
    {
      return 1;
    }
  }
  else
  {
    if (!PassUtil::ObtainCacheEncryptPassword(isSetup, user, setupAllowCacheEncrypt, pass, smtpUser, smtpPass,
                                              secretConfig, mainConfig))
    {
      return 1;
    }
  }

  if (isSetup)
  {
    std::cout << "Config saved to " << GetDisplayPath(mainConfigPath) << "\n";
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

  AppUtil::InitStdErrRedirect(logPath);

  AppUtil::SetAddressBookEncrypt(addressBookEncrypt);

  Auth::Init(auth, authEncrypt, pass, isSetup);

  std::shared_ptr<Ui> ui = std::make_shared<Ui>(inbox, address, name, prefetchLevel, prefetchAllHeaders);

  std::shared_ptr<ImapManager> imapManager =
    std::make_shared<ImapManager>(user, pass, imapHost, imapPort, online,
                                  networkTimeout,
                                  cacheEncrypt, cacheIndexEncrypt,
                                  idleTimeout,
                                  idleDelay,
                                  foldersExclude,
                                  sniEnabled,
                                  std::bind(&Ui::ResponseHandler, ui.get(), std::placeholders::_1,
                                            std::placeholders::_2),
                                  std::bind(&Ui::ResultHandler, ui.get(), std::placeholders::_1,
                                            std::placeholders::_2),
                                  std::bind(&Ui::StatusHandler, ui.get(), std::placeholders::_1),
                                  std::bind(&Ui::SearchHandler, ui.get(), std::placeholders::_1,
                                            std::placeholders::_2),
                                  idleInbox, inbox);

  std::shared_ptr<SmtpManager> smtpManager =
    std::make_shared<SmtpManager>(smtpUser, smtpPass, smtpHost, smtpPort, name, address, online,
                                  networkTimeout,
                                  std::bind(&Ui::SmtpResultHandler, ui.get(), std::placeholders::_1),
                                  std::bind(&Ui::StatusHandler, ui.get(), std::placeholders::_1));

  OfflineQueue::Init(queueEncrypt, pass);

  ui->SetImapManager(imapManager);
  ui->SetTrashFolder(trash);
  ui->SetSpamFolder(spam);
  ui->SetDraftsFolder(drafts);
  ui->SetSentFolder(sent);
  ui->SetClientStoreSent(clientStoreSent);
  ui->SetSpecialFoldersHandler([mainConfig](const std::map<std::string, std::string>& p_SpecialFolders)
  {
    // persist detected special folders
    for (const auto& specialFolder : p_SpecialFolders)
    {
      mainConfig->Set(specialFolder.first, specialFolder.second);
    }

    mainConfig->Save();
  });
  imapManager->SetDetectSpecialFolders(trash.empty() || spam.empty() || drafts.empty() || sent.empty());
  ui->SetSmtpManager(smtpManager);

  imapManager->Start();
  smtpManager->Start();

  ui->Run();

  ui->ResetSmtpManager();
  ui->ResetImapManager();

  smtpManager.reset();
  imapManager.reset();

  Auth::Cleanup();

  mainConfig->Save();
  mainConfig.reset();

  secretConfig->Save();
  secretConfig.reset();

  OfflineQueue::Cleanup();

  FileUtil::CleanupTempDir();

  AppUtil::CleanupStdErrRedirect();

  ui.reset();
  LOG_INFO("exit");

  // Save last version
  DebugInfo::SetStr("version_used", Version::GetAppVersion());
  DebugInfo::Cleanup();

  Log::Cleanup(isLogdumpEnabled);

  if (AppUtil::IsTerminateRequested())
  {
    // Re-raise so the parent sees termination by signal (shell message, exit status 128+N)
    const int sig = AppUtil::GetTerminateSignal();
    signal(sig, SIG_DFL);
    kill(getpid(), sig);
  }

  return 0;
}

static void ShowHelp()
{
  std::cout <<
    "nmail is a terminal-based email client with a user interface similar to\n"
    "alpine, supporting IMAP and SMTP.\n"
    "\n"
    "Usage: nmail [OPTION]\n"
    "\n"
    "Options:\n"
    "   -c,  --cache-encrypt       prompt for cache encryption during oauth2 setup\n"
    "   -d,  --confdir <DIR>       use a different directory than ~/.config/nmail\n"
    "   -e,  --verbose             enable verbose logging\n"
    "   -ee, --extra-verbose       enable extra verbose logging\n"
    "   -h,  --help                display this help and exit\n"
    "   -k,  --keydump             key code dump mode\n"
    "   -o,  --offline             run in offline mode\n"
    "   -p,  --pass                change password\n"
    "   -ro, --read-only           run shadow instance with read-only cache access\n"
    "   -s,  --setup [SERVICE]     setup wizard, detects service from email address\n"
    "                              if not specified\n"
    "   -t,  --theme [THEME]       install color theme, or select from list of\n"
    "                              available themes if not specified\n"
    "   -u,  --upgrade [VERSION]   upgrade release binary to latest or given version\n"
    "   -v,  --version             output version information and exit\n"
    "   -x,  --export <DIR>        export cache to specified dir in Maildir format\n"
    "\n"
    "Examples:\n"
    "   nmail -s                   setup nmail for an email account\n"
    "   nmail -t pastel-blue       install the pastel-blue color theme\n"
    "   nmail -u                   upgrade to latest release\n"
    "   nmail                      running nmail without config starts the setup\n"
    "                              wizard, alternatively nmail can be configured\n"
    "                              manually (refer to FILES section for details)\n"
    "\n"
    "Files:\n"
    "   ~/.config/nmail/auth.conf  configures custom oauth2 client id and secret\n"
    "   ~/.config/nmail/key.conf   configures user interface key bindings\n"
    "   ~/.config/nmail/main.conf  configures mail account and general settings,\n"
    "                              for full functionality the following fields\n"
    "                              shall be configured (folders are detected if\n"
    "                              not specified):\n"
    "                              address (ex: example@example.com),\n"
    "                              drafts (folder name, ex: Drafts),\n"
    "                              imap_host (ex: imap.example.com),\n"
    "                              imap_port (ex: 993),\n"
    "                              inbox (folder name, ex: Inbox),\n"
    "                              name (ex: Firstname Lastname),\n"
    "                              sent (folder name, ex: Sent),\n"
    "                              smtp_host (ex: smtp.example.com),\n"
    "                              smtp_port (ex: 465 or 587),\n"
    "                              spam (folder name, ex: Spam),\n"
    "                              trash (folder name, ex: Trash),\n"
    "                              user (ex: example@example.com or example).\n"
    "   ~/.config/nmail/ui.conf    customizes user interface settings\n"
    "\n"
    "Report bugs at https://github.com/d99kris/nmail\n"
    "\n";
}

static void ShowVersion()
{
  std::cout <<
    Version::GetAppName(true /*p_WithVersion*/) << "\n"
    "\n"
    "Copyright (c) 2019-2026 Kristofer Berggren\n"
    "\n"
    "Source is distributed under the MIT license.\n"
    "\n"
    "Binaries are distributed under the GNU GPL v2 license.\n"
    "\n"
    "Written by Kristofer Berggren.\n";
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

bool ReportConfigError(const std::string& p_Param)
{
  const std::string configPath(AppUtil::GetApplicationDir() + std::string("main.conf"));
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

  const std::string systemOs = SysUtil::GetSystemOs();
  LOG_DEBUG("system os: %s", systemOs.c_str());

  const std::string unameStr = SysUtil::GetUname();
  if (!unameStr.empty())
  {
    LOG_DEBUG("uname:   ");
    LOG_DUMP(unameStr.c_str());
  }

  const std::string libetpanVersion = SysUtil::GetLibetpanVersion();
  LOG_DEBUG("libetpan:  %s", libetpanVersion.c_str());

  const std::string saslMechs = Sasl::GetMechanismsStr();
  LOG_DEBUG("saslmechs: %s", saslMechs.c_str());

  const std::string libxapianVersion = SearchEngine::GetXapianVersion();
  LOG_DEBUG("libxapian: %s", libxapianVersion.c_str());

  const std::string openSSLVersion = Crypto::GetVersion();
  LOG_DEBUG("openssl:   %s", openSSLVersion.c_str());

  const std::string sqliteVersion = SysUtil::GetSQLiteVersion();
  LOG_DEBUG("sqlite:    %s", sqliteVersion.c_str());

  const std::string selfPath = SysUtil::GetSelfPath();
  if (!selfPath.empty())
  {
    const std::string linkedLibs = SysUtil::GetLinkedLibs(selfPath);
    if (!linkedLibs.empty())
    {
      LOG_DEBUG("libs:    ");
      LOG_DUMP(linkedLibs.c_str());
    }
  }
}

std::string GetDisplayPath(const std::string& p_Path)
{
  // shorten home dir to ~ and remove trailing slash
  std::string path = p_Path;
  if ((path.size() > 1) && (path.back() == '/'))
  {
    path.pop_back();
  }

  const char* homeEnv = getenv("HOME");
  const std::string home = (homeEnv != nullptr) ? homeEnv : "";
  if (!home.empty() && ((path == home) || StrUtil::StartsWith(path, home + "/")))
  {
    path = "~" + path.substr(home.size());
  }

  return path;
}
