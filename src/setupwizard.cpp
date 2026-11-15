// setupwizard.cpp
//
// Copyright (c) 2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "setupwizard.h"

#include <iostream>
#include <set>

#include <unistd.h>

#include "auth.h"
#include "config.h"
#include "imap.h"
#include "lineedit.h"
#include "loghelp.h"
#include "provider.h"
#include "util.h"

// value column for aligned labels
static const size_t valueColumn = 15;

bool SetupWizard::Run(const std::string& p_Setup, std::shared_ptr<Config> p_Config,
                      const std::map<std::string, std::string>& p_DefaultConfig)
{
  std::string providerId;
  std::string auth;
  if (!ResolveSetupArg(p_Setup, providerId, auth)) return false;

  std::cout << "Starting setup wizard (ctrl-c to cancel)\n\n";

  // reset account specific params, which may remain from a previous setup
  static const std::vector<std::string> accountParams =
  {
    "name", "address", "user", "imap_host", "imap_port", "smtp_host", "smtp_port", "smtp_user",
    "inbox", "trash", "spam", "drafts", "sent", "client_store_sent", "folders_exclude", "auth",
  };
  for (const auto& param : accountParams)
  {
    auto it = p_DefaultConfig.find(param);
    if (it != p_DefaultConfig.end())
    {
      p_Config->Set(param, it->second);
    }
  }

  // legacy oauth2 setup, e.g. gmail-oauth2, gets email and name from provider
  if (!auth.empty() && (auth != "pass"))
  {
    LOG_INFO("setup provider %s (arg)", providerId.c_str());
    ApplyProvider(providerId, p_Config);
    return SetupOAuth2(auth, GetParam(providerId, "oauth_note"), "", p_Config);
  }

  std::string email;
  size_t atPos = std::string::npos;
  while (true)
  {
    if (!Prompt("Email", email)) return false;

    atPos = email.find('@');
    if ((atPos != std::string::npos) && (atPos != 0) && ((atPos + 1) < email.size())) break;

    std::cout << Label("Error") << "invalid email address\n";
  }

  std::string name;
  if (!Prompt("Name", name)) return false;

  const std::string domain = Util::ToLower(email.substr(atPos + 1));
  std::string detection = "arg";
  Provider::MxResult mxResult = Provider::MxResultMatch;
  if (providerId.empty())
  {
    detection = "domain";
    providerId = Provider::FindByDomain(domain);
    if (providerId.empty())
    {
      static const std::map<Provider::MxResult, std::string> mxResultNames =
      {
        { Provider::MxResultMatch, "mx" },
        { Provider::MxResultNoMatch, "mx no match" },
        { Provider::MxResultFailed, "mx lookup failed" },
        { Provider::MxResultUnavailable, "mx lookup unavailable" },
      };
      providerId = Provider::FindByMx(domain, mxResult);
      detection = mxResultNames.at(mxResult);
    }
  }

  // log detection method, but not domain, as logs may be shared in bug reports
  LOG_INFO("setup provider %s (%s)", providerId.empty() ? "none" : providerId.c_str(), detection.c_str());

  if (!providerId.empty())
  {
    const std::map<std::string, std::string> params = Provider::GetParams(providerId);
    auto nameIt = params.find("name");
    std::cout << Label("Provider") << ((nameIt != params.end()) ? nameIt->second : providerId) << "\n";
    ApplyProvider(providerId, p_Config);

    if (auth.empty())
    {
      auto authIt = params.find("auth");
      const std::vector<std::string> auths =
        Util::Trim(Util::Split((authIt != params.end()) ? authIt->second : "pass"));
      auth = auths.empty() ? "pass" : auths.front();
      if (auths.size() > 1)
      {
        std::vector<std::string> options;
        std::vector<std::string> notes;
        for (const auto& candidate : auths)
        {
          const bool isPass = (candidate == "pass");
          const std::string hint = GetParam(providerId, isPass ? "pass_hint" : "oauth_hint");
          options.push_back(isPass ? "Password" : "OAuth2");
          notes.push_back(hint.empty() ? "" : ("(" + hint + ")"));
        }

        std::cout << "\n";
        size_t index = 0;
        if (!PromptMenu("Auth method", options, index, notes)) return false;

        auth = auths.at(index);
      }
    }

    std::cout << "\n";
    if (auth != "pass")
    {
      return SetupOAuth2(auth, GetParam(providerId, "oauth_note"), name, p_Config);
    }

    const std::string passNote = GetParam(providerId, "pass_note");
    if (!passNote.empty())
    {
      std::cout << passNote << "\n\n";
    }
  }
  else
  {
    const bool noMx = (mxResult == Provider::MxResultUnavailable);
    std::cout << Label("Provider") << "Unknown" << (noMx ? " (no MX lookup)" : "") << ", enter server details\n\n";
    if (!PromptServers(domain, p_Config)) return false;

    std::cout << "\n";
  }

  p_Config->Set("name", name);
  p_Config->Set("address", email);
  // some providers use username without domain, e.g. riseup
  const bool isLocalPart = !providerId.empty() && (GetParam(providerId, "user_format") == "localpart");
  p_Config->Set("user", isLocalPart ? email.substr(0, atPos) : email);
  p_Config->Set("auth", "pass");
  return true;
}

std::string SetupWizard::Label(const std::string& p_Name)
{
  std::string label = p_Name + ":";
  label.append((label.size() < valueColumn) ? (valueColumn - label.size()) : 1, ' ');
  return label;
}

bool SetupWizard::PromptMenu(const std::string& p_Title, const std::vector<std::string>& p_Options, size_t& p_Index,
                             const std::vector<std::string>& p_Notes)
{
  std::cout << p_Title << "\n";
  for (size_t i = 0; i < p_Options.size(); ++i)
  {
    std::string option = std::to_string(i + 1) + ". " + p_Options.at(i);
    if ((i < p_Notes.size()) && !p_Notes.at(i).empty())
    {
      option.append((option.size() < valueColumn) ? (valueColumn - option.size()) : 1, ' ');
      option += p_Notes.at(i);
    }

    std::cout << option << "\n";
  }

  while (true)
  {
    std::string value = std::to_string(p_Index + 1);
    if (!LineEdit::Read(Label("Select"), value, true /* p_ReplaceOnType */)) return false;

    const bool isNumber = !value.empty() && (value.size() < 6) &&
      (value.find_first_not_of("0123456789") == std::string::npos);
    const size_t number = isNumber ? std::stoul(value) : 0;
    if ((number >= 1) && (number <= p_Options.size()))
    {
      p_Index = number - 1;
      return true;
    }
  }
}

bool SetupWizard::PromptYesNo(const std::string& p_Name, bool& p_Value)
{
  while (true)
  {
    std::string value = p_Value ? "y" : "n";
    if (!LineEdit::Read(Label(p_Name), value, true /* p_ReplaceOnType */)) return false;

    value = Util::ToLower(value);
    if ((value == "y") || (value == "yes") || (value == "n") || (value == "no"))
    {
      p_Value = (value[0] == 'y');
      return true;
    }
  }
}

bool SetupWizard::PromptPassword(const std::string& p_Name, std::shared_ptr<Config> p_Config, std::string& p_Pass)
{
  // verify imap login, with limited attempts to avoid account lockout
  static const int maxAttempts = 3;
  static const int retryDelaySec = 2;
  uint16_t port = 0;
  int64_t timeout = 30;
  try
  {
    port = std::stoi(p_Config->Get("imap_port"));
    timeout = std::stoll(p_Config->Get("network_timeout_sec"));
  }
  catch (...)
  {
  }

  bool isLoginAttempted = false;
  for (int attempt = 1; attempt <= maxAttempts; ++attempt)
  {
    std::cout << Label(p_Name) << std::flush;
    p_Pass = Util::GetPass();
    if (p_Pass.empty())
    {
      std::cout << Label("Error") << "password not specified\n";
      continue;
    }

    // temporary progress text, erased once login check completes
    const bool isTty = isatty(STDOUT_FILENO);
    if (isTty)
    {
      std::cout << "Logging in..." << std::flush;
    }

    isLoginAttempted = true;
    std::string message;
    const Imap::LoginResult result =
      Imap::CheckLogin(p_Config->Get("user"), p_Pass, p_Config->Get("imap_host"), port, timeout,
                       (p_Config->Get("sni_enabled") == "1"), message);
    if (isTty)
    {
      std::cout << "\r\033[K" << std::flush;
    }

    if (result == Imap::LoginResultOk) return true;

    if (result == Imap::LoginResultFailed)
    {
      // not a credentials error, e.g. network issue, so proceed unverified
      std::cout << Sentence("could not verify login: " + message) << "\n";
      return true;
    }

    std::cout << Sentence(message) << "\n\n";
    if (attempt < maxAttempts)
    {
      sleep(retryDelaySec);
    }
  }

  std::cerr << "error: " << (isLoginAttempted ? "login failed" : "password not specified") << "\n";
  return false;
}

std::string SetupWizard::Sentence(const std::string& p_Text)
{
  // capitalize first letter and end with period, e.g. Invalid username or password.
  std::string sentence = p_Text;
  if (!sentence.empty() && (sentence[0] >= 'a') && (sentence[0] <= 'z'))
  {
    sentence[0] = sentence[0] - 'a' + 'A';
  }

  if (!sentence.empty() && (std::string(".!?)").find(sentence.back()) == std::string::npos))
  {
    sentence += ".";
  }

  return sentence;
}

bool SetupWizard::ResolveSetupArg(const std::string& p_Setup, std::string& p_ProviderId, std::string& p_Auth)
{
  if (p_Setup == "auto") return true;

  if (Provider::Exists(p_Setup))
  {
    p_ProviderId = p_Setup;
    return true;
  }

  // provider oauth2 auth name, e.g. gmail-oauth2
  for (const auto& id : Provider::GetIds())
  {
    const std::map<std::string, std::string> params = Provider::GetParams(id);
    auto authIt = params.find("auth");
    if (authIt == params.end()) continue;

    for (const auto& auth : Util::Trim(Util::Split(authIt->second)))
    {
      if ((auth != "pass") && (auth == p_Setup))
      {
        p_ProviderId = id;
        p_Auth = auth;
        return true;
      }
    }
  }

  std::cerr << "error: unsupported email service \"" << p_Setup << "\", supported services:\n       ";
  for (const auto& id : Provider::GetIds())
  {
    std::cerr << " " << id;
  }
  std::cerr << "\n";
  return false;
}

bool SetupWizard::SetupOAuth2(const std::string& p_Auth, const std::string& p_Note, const std::string& p_Name,
                              std::shared_ptr<Config> p_Config)
{
  if (!p_Note.empty())
  {
    std::cout << p_Note << "\n\n";
  }

  Auth::InitConfig();
  std::cout << "Waiting for web browser login (ctrl-c to cancel)\n\n" << std::flush;
  if (!Auth::GenerateToken(p_Auth))
  {
    // failure details are output by oauth2 helper
    std::cerr << "error: " << p_Auth << " setup failed\n";
    return false;
  }

  const std::string email = Auth::GetEmail();
  // name entered by user takes precedence over name from oauth2 account
  p_Config->Set("name", !p_Name.empty() ? p_Name : Auth::GetName());
  p_Config->Set("address", email);
  p_Config->Set("user", email);
  p_Config->Set("auth", p_Auth);
  return true;
}

bool SetupWizard::PromptServers(const std::string& p_Domain, std::shared_ptr<Config> p_Config)
{
  std::string imapHost = "imap." + p_Domain;
  std::string imapPort = p_Config->Get("imap_port");
  std::string smtpHost = "smtp." + p_Domain;
  std::string smtpPort = p_Config->Get("smtp_port");
  if (!Prompt("IMAP host", imapHost) || !PromptPort("IMAP port", imapPort) ||
      !Prompt("SMTP host", smtpHost) || !PromptPort("SMTP port", smtpPort)) return false;

  p_Config->Set("imap_host", imapHost);
  p_Config->Set("imap_port", imapPort);
  p_Config->Set("smtp_host", smtpHost);
  p_Config->Set("smtp_port", smtpPort);
  return true;
}

void SetupWizard::ApplyProvider(const std::string& p_ProviderId, std::shared_ptr<Config> p_Config)
{
  // provider params not part of main config
  static const std::set<std::string> providerParams =
  {
    "name", "domains", "mx", "auth", "pass_hint", "pass_note", "oauth_hint", "oauth_note", "user_format",
  };

  for (const auto& param : Provider::GetParams(p_ProviderId))
  {
    if (providerParams.count(param.first) > 0) continue;

    if (!p_Config->Exist(param.first))
    {
      LOG_WARNING("provider %s unknown param \"%s\"", p_ProviderId.c_str(), param.first.c_str());
      continue;
    }

    p_Config->Set(param.first, param.second);
  }
}

std::string SetupWizard::GetParam(const std::string& p_ProviderId, const std::string& p_Param)
{
  const std::map<std::string, std::string> params = Provider::GetParams(p_ProviderId);
  auto it = params.find(p_Param);
  return (it != params.end()) ? it->second : std::string();
}

bool SetupWizard::Prompt(const std::string& p_Name, std::string& p_Value)
{
  return LineEdit::Read(Label(p_Name), p_Value);
}

bool SetupWizard::PromptPort(const std::string& p_Name, std::string& p_Value)
{
  while (true)
  {
    if (!Prompt(p_Name, p_Value)) return false;

    const bool isPort = !p_Value.empty() && (p_Value.size() < 6) &&
      (p_Value.find_first_not_of("0123456789") == std::string::npos) && (std::stoul(p_Value) <= 65535);
    if (isPort) return true;

    std::cout << Label("Error") << "invalid port\n";
  }
}
