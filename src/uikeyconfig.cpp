// uikeyconfig.cpp
//
// Copyright (c) 2025 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "uikeyconfig.h"

#include <algorithm>
#include <fstream>

#include <ncurses.h>

#include "config.h"
#include "loghelp.h"
#include "util.h"

Config UiKeyConfig::m_Config;
std::map<std::string, int> UiKeyConfig::m_KeyCodes;

void UiKeyConfig::InitKeyCodes(bool p_MapKeys)
{
  const int keyNone = -1;
  const int keyCodeTab = 9;
  const int keyCodeReturn = 10;
  const int keyCodeSpace = 32;
  const int keyBackspaceAlt = 127;

  m_KeyCodes = std::map<std::string, int>({
    // additional keys
    { "KEY_TAB", keyCodeTab },
    { "KEY_SPACE", keyCodeSpace },
    { "KEY_RETURN", keyCodeReturn },
    { "KEY_NONE", keyNone },

    // ctrl keys
    { "KEY_CTRL@", 0 },
    { "KEY_CTRLA", 1 },
    { "KEY_CTRLB", 2 },
    { "KEY_CTRLC", 3 },
    { "KEY_CTRLD", 4 },
    { "KEY_CTRLE", 5 },
    { "KEY_CTRLF", 6 },
    { "KEY_CTRLG", 7 },
    { "KEY_CTRLH", 8 },
    { "KEY_CTRLI", 9 },
    { "KEY_CTRLJ", 10 },
    { "KEY_CTRLK", 11 },
    { "KEY_CTRLL", 12 },
    { "KEY_CTRLM", 13 },
    { "KEY_CTRLN", 14 },
    { "KEY_CTRLO", 15 },
    { "KEY_CTRLP", 16 },
    { "KEY_CTRLQ", 17 },
    { "KEY_CTRLR", 18 },
    { "KEY_CTRLS", 19 },
    { "KEY_CTRLT", 20 },
    { "KEY_CTRLU", 21 },
    { "KEY_CTRLV", 22 },
    { "KEY_CTRLW", 23 },
    { "KEY_CTRLX", 24 },
    { "KEY_CTRLY", 25 },
    { "KEY_CTRLZ", 26 },
    { "KEY_CTRL[", 27 },
    { "KEY_CTRL\\", 28 },
    { "KEY_CTRL]", 29 },
    { "KEY_CTRL^", 30 },
    { "KEY_CTRL_", 31 },

    // ncurses keys
    { "KEY_DOWN", KEY_DOWN },
    { "KEY_UP", KEY_UP },
    { "KEY_LEFT", KEY_LEFT },
    { "KEY_RIGHT", KEY_RIGHT },
    { "KEY_HOME", KEY_HOME },
    { "KEY_BACKSPACE_ALT", keyBackspaceAlt },
    { "KEY_BACKSPACE", KEY_BACKSPACE },
    { "KEY_F0", KEY_F0 },
    { "KEY_F1", KEY_F(1) },
    { "KEY_F2", KEY_F(2) },
    { "KEY_F3", KEY_F(3) },
    { "KEY_F4", KEY_F(4) },
    { "KEY_F5", KEY_F(5) },
    { "KEY_F6", KEY_F(6) },
    { "KEY_F7", KEY_F(7) },
    { "KEY_F8", KEY_F(8) },
    { "KEY_F9", KEY_F(9) },
    { "KEY_F10", KEY_F(10) },
    { "KEY_F11", KEY_F(11) },
    { "KEY_F12", KEY_F(12) },
    { "KEY_DL", KEY_DL },
    { "KEY_IL", KEY_IL },
    { "KEY_DC", KEY_DC },
    { "KEY_IC", KEY_IC },
    { "KEY_EIC", KEY_EIC },
    { "KEY_CLEAR", KEY_CLEAR },
    { "KEY_EOS", KEY_EOS },
    { "KEY_EOL", KEY_EOL },
    { "KEY_SF", KEY_SF },
    { "KEY_SR", KEY_SR },
    { "KEY_NPAGE", KEY_NPAGE },
    { "KEY_PPAGE", KEY_PPAGE },
    { "KEY_STAB", KEY_STAB },
    { "KEY_CTAB", KEY_CTAB },
    { "KEY_CATAB", KEY_CATAB },
    { "KEY_ENTER", KEY_ENTER },
    { "KEY_PRINT", KEY_PRINT },
    { "KEY_LL", KEY_LL },
    { "KEY_A1", KEY_A1 },
    { "KEY_A3", KEY_A3 },
    { "KEY_B2", KEY_B2 },
    { "KEY_C1", KEY_C1 },
    { "KEY_C3", KEY_C3 },
    { "KEY_BTAB", KEY_BTAB },
    { "KEY_BEG", KEY_BEG },
    { "KEY_CANCEL", KEY_CANCEL },
    { "KEY_CLOSE", KEY_CLOSE },
    { "KEY_COMMAND", KEY_COMMAND },
    { "KEY_COPY", KEY_COPY },
    { "KEY_CREATE", KEY_CREATE },
    { "KEY_END", KEY_END },
    { "KEY_EXIT", KEY_EXIT },
    { "KEY_FIND", KEY_FIND },
    { "KEY_HELP", KEY_HELP },
    { "KEY_MARK", KEY_MARK },
    { "KEY_MESSAGE", KEY_MESSAGE },
    { "KEY_MOVE", KEY_MOVE },
    { "KEY_NEXT", KEY_NEXT },
    { "KEY_OPEN", KEY_OPEN },
    { "KEY_OPTIONS", KEY_OPTIONS },
    { "KEY_PREVIOUS", KEY_PREVIOUS },
    { "KEY_REDO", KEY_REDO },
    { "KEY_REFERENCE", KEY_REFERENCE },
    { "KEY_REFRESH", KEY_REFRESH },
    { "KEY_REPLACE", KEY_REPLACE },
    { "KEY_RESTART", KEY_RESTART },
    { "KEY_RESUME", KEY_RESUME },
    { "KEY_SAVE", KEY_SAVE },
    { "KEY_SBEG", KEY_SBEG },
    { "KEY_SCANCEL", KEY_SCANCEL },
    { "KEY_SCOMMAND", KEY_SCOMMAND },
    { "KEY_SCOPY", KEY_SCOPY },
    { "KEY_SCREATE", KEY_SCREATE },
    { "KEY_SDC", KEY_SDC },
    { "KEY_SDL", KEY_SDL },
    { "KEY_SELECT", KEY_SELECT },
    { "KEY_SEND", KEY_SEND },
    { "KEY_SEOL", KEY_SEOL },
    { "KEY_SEXIT", KEY_SEXIT },
    { "KEY_SFIND", KEY_SFIND },
    { "KEY_SHELP", KEY_SHELP },
    { "KEY_SHOME", KEY_SHOME },
    { "KEY_SIC", KEY_SIC },
    { "KEY_SLEFT", KEY_SLEFT },
    { "KEY_SMESSAGE", KEY_SMESSAGE },
    { "KEY_SMOVE", KEY_SMOVE },
    { "KEY_SNEXT", KEY_SNEXT },
    { "KEY_SOPTIONS", KEY_SOPTIONS },
    { "KEY_SPREVIOUS", KEY_SPREVIOUS },
    { "KEY_SPRINT", KEY_SPRINT },
    { "KEY_SREDO", KEY_SREDO },
    { "KEY_SREPLACE", KEY_SREPLACE },
    { "KEY_SRIGHT", KEY_SRIGHT },
    { "KEY_SRSUME", KEY_SRSUME },
    { "KEY_SSAVE", KEY_SSAVE },
    { "KEY_SSUSPEND", KEY_SSUSPEND },
    { "KEY_SUNDO", KEY_SUNDO },
    { "KEY_SUSPEND", KEY_SUSPEND },
    { "KEY_UNDO", KEY_UNDO },
    { "KEY_MOUSE", KEY_MOUSE },
    { "KEY_RESIZE", KEY_RESIZE },
  });

  if (p_MapKeys)
  {
    std::map<std::string, std::string> keyMaps = m_Config.GetMap();
    for (auto& keyMap : keyMaps)
    {
      wint_t keyCode = UiKeyConfig::GetKey(keyMap.first);
      LOG_TRACE("cfg '%s' to use code 0x%x", keyMap.first.c_str(), keyCode);
    }
  }
}

void UiKeyConfig::Init(bool p_MapKeys)
{
  const std::map<std::string, std::string> defaultConfig =
  {
    { "key_prev_msg", "p" },
    { "key_next_msg", "n" },
    { "key_reply_all", "r" },
    { "key_reply_sender", "R" },
    { "key_forward", "f" },
    { "key_forward_attached", "F" },
    { "key_delete", "d" },
    { "key_compose", "c" },
    { "key_compose_copy", "C" },
    { "key_toggle_unread", "u" },
    { "key_move", "M" },
    { "key_auto_move", "m" },
    { "key_refresh", "l" },
    { "key_quit", "q" },
    { "key_toggle_text_html", "t" },
    { "key_cancel", "KEY_CTRLC" },
    { "key_send", "KEY_CTRLX" },
    { "key_delete_char_after_cursor", "KEY_CTRLD" },
    { "key_delete_line_after_cursor", "KEY_CTRLK" },
    { "key_delete_line_before_cursor", "KEY_CTRLU" },
    { "key_open", "." },
    { "key_back", "," },
    { "key_goto_folder", "g" },
    { "key_goto_inbox", "i" },
    { "key_to_select", "KEY_CTRLT" },
    { "key_save_file", "s" },
    { "key_ext_editor", "KEY_CTRLW" },
    { "key_ext_pager", "e" },
    { "key_postpone", "KEY_CTRLO" },
    { "key_othercmd_help", "o" },
    { "key_export", "x" },
    { "key_import", "z" },
    { "key_rich_header", "KEY_CTRLR" },
    { "key_ext_html_viewer", "v" },
    { "key_ext_html_preview", "KEY_CTRLV" },
    { "key_ext_msg_viewer", "w" },
    { "key_search", "/" },
    { "key_search_current_subject", "=" },
    { "key_search_current_name", "-" },
    { "key_find", "/" },
    { "key_find_next", "?" },
    { "key_sync", "s" },
    { "key_toggle_markdown_compose", "KEY_CTRLN" },
#if defined(__APPLE__)
    { "key_backward_word", "\\33\\142" }, // opt-left
    { "key_forward_word", "\\33\\146" }, // opt-right
    { "key_kill_word", "\\33\\50" }, // opt-delete
#else // defined(__linux__)
    { "key_backward_word", "\\4001040" }, // alt-left
    { "key_forward_word", "\\4001057" }, // alt-right
    { "key_kill_word", "\\4001006" }, // alt-delete
#endif
    { "key_backward_kill_word", "\\33\\177" }, // alt/opt-backspace
    { "key_begin_line", "KEY_CTRLA" },
    { "key_end_line", "KEY_CTRLE" },
    { "key_prev_page", "KEY_PPAGE" },
    { "key_next_page", "KEY_NPAGE" },
    { "key_prev_page_compose", "KEY_PPAGE" },
    { "key_next_page_compose", "KEY_NPAGE" },
    { "key_filter_sort_reset", "`" },
    { "key_filter_show_unread", "1" },
    { "key_filter_show_has_attachments", "2" },
    { "key_filter_show_current_date", "3" },
    { "key_filter_show_current_name", "4" },
    { "key_filter_show_current_subject", "5" },
    { "key_sort_unread", "!" },
    { "key_sort_has_attachments", "@" },
    { "key_sort_date", "#" },
    { "key_sort_name", "$" },
    { "key_sort_subject", "%" },
    { "key_jump_to", "j" },
    { "key_toggle_full_header", "h" },
    { "key_select_item", "KEY_SPACE" },
    { "key_select_all", "a" },
    { "key_search_show_folder", "\\" },
    { "key_spell", "KEY_CTRLS" },
    { "key_search_server", "'" },
    { "key_return", "KEY_RETURN" },
    { "key_enter", "KEY_ENTER" },
    { "key_left", "KEY_LEFT" },
    { "key_right", "KEY_RIGHT" },
    { "key_down", "KEY_DOWN" },
    { "key_up", "KEY_UP" },
    { "key_end", "KEY_END" },
    { "key_home", "KEY_HOME" },
    { "key_backspace", "KEY_BACKSPACE" },
    { "key_backspace_alt", "KEY_BACKSPACE_ALT" },
    { "key_delete_char", "KEY_DC" },
    { "key_space", "KEY_SPACE" },
    { "key_tab", "KEY_TAB" },
    { "key_terminal_resize", "KEY_RESIZE" },
  };

  const std::string keyConfigPath(Util::GetApplicationDir() + std::string("key.conf"));

  // @todo: remove legacy migration of old config after one year ~Jan 2026
  if (!Util::Exists(keyConfigPath))
  {
    const std::string configPath(Util::GetApplicationDir() + std::string("ui.conf"));
    if (Util::Exists(configPath))
    {
      MigrateFromUiConfig(configPath, keyConfigPath);
    }
  }

  m_Config = Config(keyConfigPath, defaultConfig);

  DetectConflicts();
  InitKeyCodes(p_MapKeys);
}

void UiKeyConfig::Cleanup()
{
  m_Config.Save();
}

std::string UiKeyConfig::GetStr(const std::string& p_Param)
{
  return m_Config.Get(p_Param);
}

int UiKeyConfig::GetKey(const std::string& p_Param)
{
  return GetKeyCode(m_Config.Get(p_Param));
}

std::string UiKeyConfig::GetKeyName(int p_KeyCode)
{
  static const std::map<int, std::string> s_KeyNames = []()
  {
    std::map<int, std::string> keyNames;
    for (auto& nameCodePair : m_KeyCodes)
    {
      keyNames[GetOffsettedKeyCode(nameCodePair.second)] = nameCodePair.first;
    }

    return keyNames;
  }();

  std::string keyName;
  std::map<int, std::string>::const_iterator it = s_KeyNames.find(p_KeyCode);
  if (it != s_KeyNames.end())
  {
    keyName = it->second;
  }

  return keyName;
}

std::map<std::string, std::string> UiKeyConfig::GetMap()
{
  return m_Config.GetMap();
}

int UiKeyConfig::GetOffsettedKeyCode(int p_KeyCode, bool p_IsFunctionKey)
{
  static const int functionKeyOffset = UiKeyConfig::GetFunctionKeyOffset();
  return p_KeyCode | (p_IsFunctionKey ? functionKeyOffset : 0x0);
}

int UiKeyConfig::GetOffsettedKeyCode(int p_KeyCode)
{
  static const int functionKeyOffset = UiKeyConfig::GetFunctionKeyOffset();
  return p_KeyCode | ((p_KeyCode > 0xff) ? functionKeyOffset : 0x0);
}

int UiKeyConfig::GetKeyCode(const std::string& p_KeyName)
{
  int keyCode = -1;
  std::map<std::string, int>::iterator it = m_KeyCodes.find(p_KeyName);
  if (it != m_KeyCodes.end())
  {
    keyCode = GetOffsettedKeyCode(it->second);
    LOG_TRACE("map '%s' to code 0x%x", p_KeyName.c_str(), keyCode);
  }
  else if ((p_KeyName.size() > 2) && (p_KeyName.substr(0, 2) == "0x"))
  {
    keyCode = strtol(p_KeyName.c_str(), 0, 16);
    LOG_TRACE("map '%s' to code 0x%x", p_KeyName.c_str(), keyCode);
  }
  else if ((p_KeyName.size() == 1) && isprint((int)p_KeyName.at(0)))
  {
    keyCode = (int)p_KeyName.at(0);
    LOG_TRACE("map '%s' to code 0x%x", p_KeyName.c_str(), keyCode);
  }
  else if ((p_KeyName.size() > 1) && (p_KeyName.substr(0, 1) == "\\"))
  {
    if (std::count(p_KeyName.begin(), p_KeyName.end(), '\\') > 1)
    {
      keyCode = GetOffsettedKeyCode(GetVirtualKeyCodeFromOct(p_KeyName));
    }
    else
    {
      std::string valstr = p_KeyName.substr(1);
      keyCode = strtol(valstr.c_str(), 0, 8);
    }

    LOG_TRACE("map '%s' to code 0x%x", p_KeyName.c_str(), keyCode);
  }
  else
  {
    LOG_WARNING("unknown key \"%s\"", p_KeyName.c_str());
  }

  return keyCode;
}

int UiKeyConfig::GetVirtualKeyCodeFromOct(const std::string& p_KeyOct)
{
  static std::map<std::string, int> reservedVirtualKeyCodes;
  auto it = reservedVirtualKeyCodes.find(p_KeyOct);
  if (it != reservedVirtualKeyCodes.end())
  {
    return it->second;
  }
  else
  {
    int keyCode = ReserveVirtualKeyCode();
    std::string keyStr = Util::StrFromOct(p_KeyOct);
    define_key(keyStr.c_str(), keyCode);
    LOG_TRACE("define '%s' code 0x%x", p_KeyOct.c_str(), keyCode);
    reservedVirtualKeyCodes[p_KeyOct] = keyCode;
    return keyCode;
  }
}

int UiKeyConfig::ReserveVirtualKeyCode()
{
  // Using Unicode's first Private Use Area (U+E000–U+F8FF) and starting at a
  // code point currently not identified as used by any vendor in
  // https://en.wikipedia.org/wiki/Private_Use_Areas
  // (side-note: wchar_t is UTF-32 on Linux/Mac, i.e. equal to Unicode code points.)
  static int keyCode = 0xF300;
  return keyCode++;
}

int UiKeyConfig::GetFunctionKeyOffset()
{
  // Using Unicode's supplementary Private Use Area B (U+100000..U+10FFFD).
  return 0x100000;
}

void UiKeyConfig::DetectConflicts()
{
  // Simplified conflict detection, not taking into account different modes / views, for now.
  std::set<std::string> ignoreKeyFunctions =
  {
    "key_next_page_compose",
    "key_prev_page_compose",
    "key_save_file",
    "key_search",
    "key_space",
  };
  std::set<std::string> dupeMappings;
  std::map<std::string, std::vector<std::string>> keyMappings;
  std::map<std::string, std::string> keyMap = m_Config.GetMap();
  for (auto it = keyMap.begin(); it != keyMap.end(); ++it)
  {
    const std::string& keyFunction = it->first;
    const std::string& keyCode = it->second;

    if (ignoreKeyFunctions.count(keyFunction)) continue;

    std::vector<std::string>& keyMapping = keyMappings[keyCode];
    keyMapping.push_back(keyFunction);
    if (keyMapping.size() > 1)
    {
      dupeMappings.insert(keyCode);
    }
  }

  for (auto it = dupeMappings.begin(); it != dupeMappings.end(); ++it)
  {
    const std::string& keyCode = *it;
    const std::vector<std::string>& keyMapping = keyMappings[keyCode];
    const std::string keyFunctions = Util::Join(keyMapping, ", ");
    LOG_WARNING("key \"%s\" has duplicate mappings: %s", keyCode.c_str(), keyFunctions.c_str());
  }
}

void UiKeyConfig::MigrateFromUiConfig(const std::string& p_UiConfigPath, const std::string& p_KeyConfigPath)
{
  std::ifstream istream;
  istream.open(p_UiConfigPath, std::ios::binary);
  if (istream.fail()) return;

  std::ofstream ostream(p_KeyConfigPath, std::ios::binary);
  if (ostream.fail()) return;

  // Skip migration of key bindings likely using opt/alt key codes, which will change
  // with function key handling implementation.
  const std::set<std::string> skip_migration =
  {
    "key_backward_word",
    "key_forward_word",
    "key_backward_kill_word",
    "key_kill_word",
  };

  std::string line;
  while (std::getline(istream, line))
  {
    if (line.rfind("key_", 0) != 0) continue; // skip non key_ params

    std::string param;
    std::istringstream linestream(line);
    if (!std::getline(linestream, param, '=')) continue;

    if (skip_migration.count(param) > 0) continue;

    ostream << line << "\n";
  }
}
