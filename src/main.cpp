// main.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

#include <unistd.h>

#include "apathy/path.hpp"

#include "addressbook.h"
#include "auth.h"
#include "cacheutil.h"
#include "config.h"
#include "crypto.h"
#include "debuginfo.h"
#include "imapmanager.h"
#include "lockfile.h"
#include "log.h"
#include "loghelp.h"
#include "offlinequeue.h"
#include "sasl.h"
#include "sethelp.h"
#include "setupwizard.h"
#include "smtpmanager.h"
#include "theme.h"
#include "ui.h"
#include "uikeyconfig.h"
#include "uikeyinput.h"
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
static int Upgrade(const std::string& p_Version, mode_t p_Umask);
static void LogSystemInfo();
static bool ChangePasswords(std::shared_ptr<Config> p_MainConfig,
                            std::shared_ptr<Config> p_SecretConfig);
static bool ChangeCachePasswords(std::shared_ptr<Config> p_MainConfig,
                                 const std::string& p_OldPass, const std::string& p_NewPass);
static void KeyDump();
static std::string GetDisplayPath(const std::string& p_Path);

int main(int argc, char* argv[])
{
  // Defaults
  const mode_t origUmask = umask(S_IRWXG | S_IRWXO);
  Util::SetApplicationDir(Util::GetDefaultApplicationDir());
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
    return Upgrade(upgradeVersion, origUmask);
  }

  // start setup wizard on first run
  const std::string mainConfigFile = Util::GetApplicationDir() + std::string("main.conf");
  const bool hasMainConfig = apathy::Path(mainConfigFile).exists();
  if (setup.empty() && !readOnly && !changePass && !keyDump && exportDir.empty() && !hasMainConfig &&
      isatty(STDIN_FILENO))
  {
    std::cout << "No config found in " << GetDisplayPath(Util::GetApplicationDir()) << "\n\n";
    setup = "auto";
  }

  const bool isSetup = !setup.empty();

  std::shared_ptr<ScopedDirLock> dirLock;
  if (!readOnly)
  {
    if (!apathy::Path(Util::GetApplicationDir()).exists())
    {
      apathy::Path::makedirs(Util::GetApplicationDir());
    }

    dirLock.reset(new ScopedDirLock(Util::GetApplicationDir()));
    if (!dirLock->IsLocked())
    {
      readOnly = true;
    }
  }

  if (readOnly && (isSetup || changePass))
  {
    std::cerr <<
      "error: setup and password change require exclusive access to config dir\n"
      "       " << Util::GetApplicationDir() << "\n" <<
      "       ensure no other nmail instance is running, and -ro is not specified.\n";
    return 1;
  }

  // Note: must be set before any Config is constructed, as Config write access depends on it
  Util::SetReadOnly(readOnly);

  const std::string& logPath = Util::GetApplicationDir() + std::string("log.txt");
  Log::SetPath(logPath);

  THREAD_REGISTER();
  Util::InitAppSignalHandlers();

  const std::string appVersion = Version::GetAppName(true /*p_WithVersion*/, true /*p_WithBranch*/);
  LOG_INFO("%s", appVersion.c_str());
  std::string osArch = Util::GetOsArch(false /*p_Verbose*/);
  LOG_INFO("%s", osArch.c_str());
  std::string buildInfo = Util::GetBuildInfo();
  LOG_INFO("%s", buildInfo.c_str());
  std::string compiler = Util::GetCompiler(false /*p_Verbose*/);
  LOG_INFO("%s", compiler.c_str());

  if (readOnly)
  {
    LOG_INFO("read-only mode");
  }

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
  const std::string mainConfigPath(Util::GetApplicationDir() + std::string("main.conf"));
  std::shared_ptr<Config> mainConfig =
    std::make_shared<Config>(mainConfigPath, defaultMainConfig, false, renamedMainConfig);

  const std::string secretConfigPath(Util::GetApplicationDir() + std::string("secret.conf"));

  if (keyDump)
  {
    KeyDump();
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
  Util::SetBrowserCmd(mainConfig->Get("browser_cmd"));

  if (isSetup)
  {
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
  std::string spam = mainConfig->Get("spam");
  std::string drafts = mainConfig->Get("drafts");
  std::string sent = mainConfig->Get("sent");
  const bool clientStoreSent = (mainConfig->Get("client_store_sent") == "1");
  const bool idleInbox = (mainConfig->Get("idle_inbox") == "1");
  Util::SetHtmlToTextConvertCmd(mainConfig->Get("html_to_text_cmd"));
  Util::SetTextToHtmlConvertCmd(mainConfig->Get("text_to_html_cmd"));
  Util::SetPartsViewerCmd(mainConfig->Get("parts_viewer_cmd"));
  Util::SetHtmlViewerCmd(mainConfig->Get("html_viewer_cmd"));
  Util::SetHtmlPreviewCmd(mainConfig->Get("html_preview_cmd"));
  Util::SetMsgViewerCmd(mainConfig->Get("msg_viewer_cmd"));
  Util::SetLinkOpenCmd(mainConfig->Get("link_open_cmd"));
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
  const bool isCoredumpEnabled = (mainConfig->Get("coredump_enabled") == "1");
  const bool sniEnabled = (mainConfig->Get("sni_enabled") == "1");
  const bool isLogdumpEnabled = (mainConfig->Get("logdump_enabled") == "1");
  Util::SetCopyToTrash(mainConfig->Get("copy_to_trash"), mainConfig->Get("imap_host"));
  mainConfig->Set("copy_to_trash", std::to_string(Util::GetCopyToTrash()));
  Util::SetAssertAbort(mainConfig->Get("assert_abort") == "1");

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
    Util::InitCoredump();
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
    if (!ObtainCacheEncryptPassword(isSetup, user, setupAllowCacheEncrypt, pass, smtpUser, smtpPass, secretConfig,
                                    mainConfig))
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

  Util::InitStdErrRedirect(logPath);

  Util::SetAddressBookEncrypt(addressBookEncrypt);

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

  Util::CleanupTempDir();

  Util::CleanupStdErrRedirect();

  ui.reset();
  LOG_INFO("exit");

  // Save last version
  DebugInfo::SetStr("version_used", Version::GetAppVersion());
  DebugInfo::Cleanup();

  Log::Cleanup(isLogdumpEnabled);

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

static int Upgrade(const std::string& p_Version, mode_t p_Umask)
{
  // exit status 0 upgraded, 3 up to date (from install.sh), 5 failed
  static const int failed = 5;
#if defined(NMAIL_BUILD_STATIC_EXTLIBS)
  const bool isRelease = (Version::GetBuildOrigin() == "github");
#else
  const bool isRelease = false;
#endif
  if (!isRelease)
  {
    std::cerr <<
      "error: upgrade is only supported for pre-built release binaries, upgrade\n"
      "       using the package manager or method nmail was installed with.\n";
    return failed;
  }

  // installed by install.sh as <prefix>/bin/nmail
  char realPath[PATH_MAX];
  const std::string selfPath = Util::GetSelfPath();
  if (selfPath.empty() || (realpath(selfPath.c_str(), realPath) == nullptr))
  {
    std::cerr << "error: cannot determine nmail binary path\n";
    return failed;
  }

  const std::string binPath = realPath;
  const std::string binDir = binPath.substr(0, binPath.rfind('/'));
  const size_t binDirPos = binDir.rfind('/');
  if ((binDirPos == std::string::npos) || (binDir.substr(binDirPos + 1) != "bin"))
  {
    std::cerr <<
      "error: nmail binary " << realPath << " is not in a bin dir,\n"
      "       re-install using install.sh\n";
    return failed;
  }

  if (access(binDir.c_str(), W_OK) != 0)
  {
    std::cerr <<
      "error: install dir " << binDir << " is not writable by the current user,\n"
      "       re-run with sudo, e.g.: sudo nmail -u\n";
    return failed;
  }

  const std::string prefix = (binDirPos == 0) ? "/" : binDir.substr(0, binDirPos);

  // NMAIL_INSTALL_URL allows testing a local install.sh, e.g. file://$PWD/utils/install.sh
  const char* envUrl = getenv("NMAIL_INSTALL_URL");
  const std::string url = ((envUrl != nullptr) && (*envUrl != '\0')) ? envUrl :
    "https://raw.githubusercontent.com/d99kris/nmail/master/utils/install.sh";

  // values passed as positional args to avoid shell quoting
  static const std::string script =
    "if command -v curl >/dev/null 2>&1; then s=\"$(curl -fsSL \"$1\")\"\n"
    "elif command -v wget >/dev/null 2>&1; then s=\"$(wget -qO- \"$1\")\"\n"
    "else echo \"error: need curl or wget to download\" >&2; exit 5; fi ||\n"
    "  { echo \"error: failed to download $1\" >&2; exit 5; }\n"
    "NMAIL_PREFIX=\"$2\" NMAIL_VERSION=\"$3\" NMAIL_CURRENT_VERSION=\"$4\" \\\n"
    "  exec bash -c \"$s\" install.sh\n";

  // exec so the running binary is not held during install, with user umask restored
  const std::string curVersion = Version::GetAppVersion();
  umask(p_Umask);
  execlp("bash", "bash", "-c", script.c_str(), "nmail-upgrade", url.c_str(), prefix.c_str(),
         p_Version.c_str(), curVersion.c_str(), (char*)nullptr);
  std::cerr << "error: failed to run bash: " << strerror(errno) << "\n";
  return failed;
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
    const std::string label = p_SmtpUser.empty() ? "Password" : "IMAP Password";
    if (p_IsSetup)
    {
      if (!SetupWizard::PromptPassword(label, p_MainConfig, p_Pass)) return false;
    }
    else
    {
      std::cout << label << ": " << std::flush;
      p_Pass = Util::GetPass();
    }

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
      std::cout << (p_IsSetup ? SetupWizard::Label("SMTP Password") : "SMTP Password: ") << std::flush;
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

static bool ObtainCacheEncryptPassword(const bool p_IsSetup, const std::string& p_User,
                                       bool p_SetupAllowCacheEncrypt,
                                       std::string& p_Pass, std::string& p_SmtpUser, std::string& p_SmtpPass,
                                       std::shared_ptr<Config> p_SecretConfig, std::shared_ptr<Config> p_MainConfig)
{
  if (p_IsSetup)
  {
    if (p_SetupAllowCacheEncrypt)
    {
      std::cout << SetupWizard::Label("Cache Encryption Password (optional)") << std::flush;
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
        std::string encPass = Util::ToHex(Crypto::AESEncrypt(p_Pass, p_User));
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

  UiKeyConfig::Init(false);

  bool running = true;
  while (running)
  {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    int maxfd = STDIN_FILENO;
    struct timeval tv = { 1, 0 };
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
      while (UiKeyInput::GetWch(&key) != ERR)
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
        std::string keyName = UiKeyConfig::GetKeyName(keyOk);
        if (!keyName.empty())
        {
          printw(" %s", keyName.c_str());
        }
      }

      printw("\n");
      refresh();
    }
  }

  UiKeyConfig::Cleanup();
  wclear(stdscr);
  endwin();
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
  if (!home.empty() && ((path == home) || Util::StartsWith(path, home + "/")))
  {
    path = "~" + path.substr(home.size());
  }

  return path;
}
