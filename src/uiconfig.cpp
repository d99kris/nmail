// uiconfig.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uiconfig.h"

#include <map>

#include "apputil.h"
#include "strutil.h"

Config UiConfig::m_Config;

void UiConfig::Init()
{
  const std::map<std::string, std::string> defaultConfig =
  {
    { "compose_line_wrap", "0" },
    { "respect_format_flowed", "1" },
    { "rewrap_quoted_lines", "1" },
    { "help_enabled", "1" },
    { "picker_width", "58" },
    { "persist_file_selection_dir", "1" },
    { "persist_find_query", "0" },
    { "persist_folder_filter", "1" },
    { "persist_search_query", "0" },
    { "plain_text", "0" },
    { "show_progress", "1" },
    { "new_msg_bell", "1" },
    { "quit_without_confirm", "1" },
    { "send_without_confirm", "0" },
    { "cancel_without_confirm", "0" },
    { "postpone_without_confirm", "0" },
    { "delete_without_confirm", "0" },
    { "show_embedded_images", "1" },
    { "show_rich_header", "0" },
    { "markdown_html_compose", "0" },
    { "markdown_view", "1" },
    { "markdown_view_links", "1" },
    { "plain_view_links", "1" },
    { "align_header_labels", "1" },
    { "colors_enabled", "1" },
    { "attachment_indicator", "\xF0\x9F\x93\x8E" },
    { "bottom_reply", "0" },
    { "compose_backup_interval_sec", "10" },
    { "persist_sortfilter", "1" },
    { "persist_selection_on_sortfilter_change", "1" },
    { "unread_indicator", "N" },
    { "invalid_input_notify", "1" },
    { "full_header_include_local", "0" },
    { "show_compact_header", "1" },
    { "tab_size", "8" },
    { "search_show_folder", "1" },
    { "localized_subject_prefixes", "" },
    { "signature", "0" },
    { "terminal_title", "" },
    { "top_bar_show_version", "0" },
    { "unwrap_quoted_lines", "1" },
    { "automove_trash_allow", "1" },
    { "top_bar_show_message_count", "0" },
  };
  const std::map<std::string, std::string> renamedConfig =
  {
    { "compose_backup_interval", "compose_backup_interval_sec" },
  };
  const std::string configPath(AppUtil::GetApplicationDir() + std::string("ui.conf"));
  m_Config = Config(configPath, defaultConfig, false, renamedConfig);
  m_Config.LogParams();
}

void UiConfig::Cleanup()
{
  m_Config.Save();
}

bool UiConfig::GetBool(const std::string& p_Param)
{
  return m_Config.Get(p_Param) == "1";
}

void UiConfig::SetBool(const std::string& p_Param, const bool& p_Value)
{
  m_Config.Set(p_Param, p_Value ? "1" : "0");
}

std::string UiConfig::GetStr(const std::string& p_Param)
{
  return m_Config.Get(p_Param);
}

int UiConfig::GetNum(const std::string& p_Param)
{
  return StrUtil::ToInteger(m_Config.Get(p_Param));
}
