// mainconfig.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "mainconfig.h"

#include "apputil.h"

std::shared_ptr<Config> MainConfig::m_Config;

void MainConfig::Init()
{
  const std::map<std::string, std::string> renamedConfig =
  {
    { "idle_timeout", "idle_timeout_min" },
    { "network_timeout", "network_timeout_sec" },
  };
  m_Config = std::make_shared<Config>(GetPath(), GetDefaultConfig(), false, renamedConfig);
}

void MainConfig::Cleanup()
{
  m_Config->Save();
  m_Config.reset();
}

std::string MainConfig::GetPath()
{
  return AppUtil::GetApplicationDir() + std::string("main.conf");
}

const std::map<std::string, std::string>& MainConfig::GetDefaultConfig()
{
  static const std::map<std::string, std::string> defaultConfig =
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
  return defaultConfig;
}

std::shared_ptr<Config> MainConfig::GetConfig()
{
  return m_Config;
}

bool MainConfig::GetBool(const std::string& p_Param)
{
  return m_Config->Get(p_Param) == "1";
}

std::string MainConfig::GetStr(const std::string& p_Param)
{
  return m_Config->Get(p_Param);
}

void MainConfig::SetStr(const std::string& p_Param, const std::string& p_Value)
{
  m_Config->Set(p_Param, p_Value);
}

void MainConfig::Save()
{
  m_Config->Save();
}

void MainConfig::LogParamsExcept(const std::set<std::string>& p_Exclude)
{
  m_Config->LogParamsExcept(p_Exclude);
}
