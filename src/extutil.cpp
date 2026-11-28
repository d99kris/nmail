// extutil.cpp
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include "extutil.h"

#include <algorithm>
#include <cstdlib>
#include <functional>

#include "nhtmlmd.h"

#include "fileutil.h"
#include "mainconfig.h"
#include "strutil.h"
#include "sysutil.h"

bool ExtUtil::m_HtmlToTextLinks = false;

// settings are read from main config at first use
static const std::string& ConfigHtmlToTextConvertCmd()
{
  static const std::string cmd = MainConfig::GetStr("html_to_text_cmd");
  return cmd;
}

static const std::string& ConfigTextToHtmlConvertCmd()
{
  static const std::string cmd = MainConfig::GetStr("text_to_html_cmd");
  return cmd;
}

static const std::string& ConfigPartsViewerCmd()
{
  static const std::string cmd = MainConfig::GetStr("parts_viewer_cmd");
  return cmd;
}

static const std::string& ConfigHtmlPreviewCmd()
{
  static const std::string cmd = MainConfig::GetStr("html_preview_cmd");
  return cmd;
}

static const std::string& ConfigHtmlViewerCmd()
{
  static const std::string cmd = MainConfig::GetStr("html_viewer_cmd");
  return cmd;
}

static const std::string& ConfigMsgViewerCmd()
{
  static const std::string cmd = MainConfig::GetStr("msg_viewer_cmd");
  return cmd;
}

static const std::string& ConfigLinkOpenCmd()
{
  static const std::string cmd = MainConfig::GetStr("link_open_cmd");
  return cmd;
}

static const std::string& ConfigPagerCmd()
{
  static const std::string cmd = MainConfig::GetStr("pager_cmd");
  return cmd;
}

static const std::string& ConfigEditorCmd()
{
  static const std::string cmd = MainConfig::GetStr("editor_cmd");
  return cmd;
}

static const std::string& ConfigSpellCmd()
{
  static const std::string cmd = MainConfig::GetStr("spell_cmd");
  return cmd;
}

static const std::string& ConfigFilePickerCmd()
{
  static const std::string cmd = MainConfig::GetStr("file_picker_cmd");
  return cmd;
}

std::string ExtUtil::GetHtmlToTextConvertCmd()
{
  return ConfigHtmlToTextConvertCmd();
}

size_t ExtUtil::GetHtmlToTextConvertCmdHash()
{
  // built-in converter output changes with its version
  static std::hash<std::string> hashStr;
  const std::string& htmlToTextCmd = GetHtmlToTextConvertCmd();
  const std::string converter = htmlToTextCmd.empty() ?
    ("nhtmlmd " + Nhtmlmd::GetVersion() + (m_HtmlToTextLinks ? " refs" : "")) : htmlToTextCmd;
  return hashStr(converter);
}

void ExtUtil::SetHtmlToTextLinks(bool p_HtmlToTextLinks)
{
  m_HtmlToTextLinks = p_HtmlToTextLinks;
}

std::string ExtUtil::ConvertHtmlToText(const std::string& p_Html)
{
  const std::string& htmlToTextCmd = GetHtmlToTextConvertCmd();
  if (htmlToTextCmd.empty())
  {
    Nhtmlmd::HtmlOptions options;
    options.m_LinkStyle = m_HtmlToTextLinks ? Nhtmlmd::LinkStyleRefs : Nhtmlmd::LinkStyleText;
    return Nhtmlmd::HtmlToMarkdown(p_Html, options);
  }

  // @todo: more elegant removal of meta-tags
  std::string html = p_Html;
  StrUtil::ReplaceString(html, "<meta ", "<beta ");
  StrUtil::ReplaceString(html, "<META ", "<BETA ");

  const std::string& tempPath = FileUtil::GetTempFilename(".html");
  FileUtil::WriteFile(tempPath, html);
  const std::string& cmd = MakeCmd(htmlToTextCmd, tempPath);
  const std::string& text = SysUtil::RunCommand(cmd);
  FileUtil::DeleteFile(tempPath);

  return text;
}

std::string ExtUtil::GetTextToHtmlConvertCmd()
{
  return ConfigTextToHtmlConvertCmd();
}

std::string ExtUtil::ConvertTextToHtml(const std::string& p_Text)
{
  const std::string& textToHtmlCmd = GetTextToHtmlConvertCmd();
  if (textToHtmlCmd.empty())
  {
    Nhtmlmd::MarkdownOptions options;
    options.m_HardBreaks = true;
    return Nhtmlmd::MarkdownToHtml(p_Text, options);
  }

  std::string text = p_Text;
  StrUtil::ReplaceString(text, "\n", "  \n"); // prepend line-breaks with double spaces to enforce them
  const std::string& tempPath = FileUtil::GetTempFilename(".md");
  FileUtil::WriteFile(tempPath, text);
  const std::string& cmd = MakeCmd(textToHtmlCmd, tempPath);
  const std::string& htmlText = SysUtil::RunCommand(cmd);
  FileUtil::DeleteFile(tempPath);

  return htmlText;
}

std::string ExtUtil::GetPartsViewerCmd()
{
  if (!ConfigPartsViewerCmd().empty()) return ConfigPartsViewerCmd();

  static std::string defaultPartsViewerCmd = GetDefaultPartsViewerCmd();

  return defaultPartsViewerCmd;
}

std::string ExtUtil::GetDefaultPartsViewerCmd()
{
#if defined(__APPLE__)
  return "open '%1'";
#elif defined(__linux__)
  return "xdg-open '%1' >/dev/null 2>&1";
#else
  return "";
#endif
}

bool ExtUtil::IsDefaultPartsViewerCmd()
{
  return ConfigPartsViewerCmd().empty();
}

std::string ExtUtil::GetHtmlViewerCmd()
{
  if (!ConfigHtmlViewerCmd().empty()) return ConfigHtmlViewerCmd();

  static std::string defaultHtmlViewerCmd = GetDefaultHtmlViewerCmd();

  return defaultHtmlViewerCmd;
}

std::string ExtUtil::GetDefaultHtmlViewerCmd()
{
  std::string result;
  const std::string& commandOutPath = FileUtil::GetTempFilename(".txt");
  const std::string& command = std::string("which w3m lynx elinks 2> /dev/null | head -1 > ") + commandOutPath;
  if (SysUtil::System(command) == 0)
  {
    std::string output = FileUtil::ReadFile(commandOutPath);
    output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
    if (!output.empty())
    {
      if (output.find("/w3m") != std::string::npos)
      {
        result =
          "LESSOPEN=\"|$(which lesspipe.sh lesspipe | head -1) %s\" w3m -o confirm_qq=0 -o use_mouse=0 -o use_lessopen=1 '%1'";
      }
      else if (output.find("/lynx") != std::string::npos)
      {
        result = "lynx '%1'";
      }
      else if (output.find("/elinks") != std::string::npos)
      {
        result = "elinks '%1'";
      }
    }
  }

  FileUtil::DeleteFile(commandOutPath);

  if (result.empty())
  {
    result = GetSystemOpenCmd();
  }

  return result;
}

std::string ExtUtil::GetSystemOpenCmd()
{
#if defined(__APPLE__)
  return "open '%1'";
#elif defined(__linux__)
  return "xdg-open '%1' >/dev/null 2>&1";
#else
  return "xdg-open '%1' >/dev/null 2>&1";
#endif
}

bool ExtUtil::IsHtmlViewerSystemOpenCmd()
{
  static bool isHtmlViewerSystemOpenCmd = (MakeCmd(GetHtmlViewerCmd(), "") == MakeCmd(GetSystemOpenCmd(), ""));
  return isHtmlViewerSystemOpenCmd;
}

std::string ExtUtil::GetHtmlPreviewCmd()
{
  if (!ConfigHtmlPreviewCmd().empty()) return ConfigHtmlPreviewCmd();

  static std::string defaultHtmlPreviewCmd = GetDefaultHtmlPreviewCmd();

  return defaultHtmlPreviewCmd;
}

std::string ExtUtil::GetDefaultHtmlPreviewCmd()
{
#if defined(__APPLE__)
  return "open '%1'";
#elif defined(__linux__)
  return "xdg-open '%1' >/dev/null 2>&1";
#else
  return "";
#endif
}

bool ExtUtil::IsDefaultHtmlPreviewCmd()
{
  return ConfigHtmlPreviewCmd().empty();
}

std::string ExtUtil::GetMsgViewerCmd()
{
  if (!ConfigMsgViewerCmd().empty()) return ConfigMsgViewerCmd();

  static std::string defaultMsgViewerCmd = GetDefaultMsgViewerCmd();

  return defaultMsgViewerCmd;
}

std::string ExtUtil::GetDefaultMsgViewerCmd()
{
#if defined(__APPLE__)
  return "open '%1'";
#elif defined(__linux__)
  return "xdg-open '%1' >/dev/null 2>&1";
#else
  return "";
#endif
}

bool ExtUtil::IsDefaultMsgViewerCmd()
{
  return ConfigMsgViewerCmd().empty();
}

std::string ExtUtil::GetLinkOpenCmd()
{
  if (!ConfigLinkOpenCmd().empty()) return ConfigLinkOpenCmd();

  static std::string defaultLinkOpenCmd = GetDefaultLinkOpenCmd();

  return defaultLinkOpenCmd;
}

std::string ExtUtil::GetDefaultLinkOpenCmd()
{
#if defined(__APPLE__)
  return "open '%1'";
#elif defined(__linux__)
  return "xdg-open '%1' >/dev/null 2>&1";
#else
  return "";
#endif
}

bool ExtUtil::IsDefaultLinkOpenCmd()
{
  return ConfigLinkOpenCmd().empty();
}

std::string ExtUtil::MakeCmd(const std::string& p_Cmd, const std::string& p_Arg)
{
  // replace %1 with single-quoted argument, append if not present
  std::string cmd = p_Cmd;
  StrUtil::ReplaceString(cmd, "'%1'", "%1");
  StrUtil::ReplaceString(cmd, "\"%1\"", "%1");
  if (cmd.find("%1") == std::string::npos)
  {
    cmd += " %1";
  }

  StrUtil::ReplaceString(cmd, "%1", "'" + EscapeSingleQuote(p_Arg) + "'");
  return cmd;
}

std::string ExtUtil::GetEditorCmd()
{
  if (!ConfigEditorCmd().empty()) return ConfigEditorCmd();

  return std::string(getenv("EDITOR") ? getenv("EDITOR") : "nano");
}

std::string ExtUtil::GetSpellCmd()
{
  static const std::string cmd = []()
  {
    std::string spellCheckCommand = ConfigSpellCmd();
    if (spellCheckCommand.empty())
    {
      const std::string& commandOutPath = FileUtil::GetTempFilename(".txt");
      const std::string& whichCommand =
        std::string("which aspell ispell 2> /dev/null | head -1 > ") + commandOutPath;

      if (SysUtil::System(whichCommand) == 0)
      {
        std::string output = FileUtil::ReadFile(commandOutPath);
        output.erase(std::remove(output.begin(), output.end(), '\n'), output.end());
        if (!output.empty())
        {
          if (output.find("/aspell") != std::string::npos)
          {
            spellCheckCommand = "aspell -c";
          }
          else if (output.find("/ispell") != std::string::npos)
          {
            spellCheckCommand = "ispell -o -x";
          }
        }
      }

      FileUtil::DeleteFile(commandOutPath);
    }

    return spellCheckCommand;
  }();

  return cmd;
}

std::string ExtUtil::GetPagerCmd()
{
  if (!ConfigPagerCmd().empty()) return ConfigPagerCmd();

  return std::string(getenv("PAGER") ? getenv("PAGER") : "less");
}

void ExtUtil::SetBrowserCmd(const std::string& p_BrowserCmd)
{
  if (p_BrowserCmd.empty()) return;

  setenv("BROWSER", p_BrowserCmd.c_str(), 1);
}

std::string ExtUtil::GetFilePickerCmd()
{
  return ConfigFilePickerCmd();
}

std::string ExtUtil::EscapeSingleQuote(const std::string& p_Str)
{
  // for use inside single-quoted shell argument
  std::string text = p_Str;
  StrUtil::ReplaceString(text, "'", "'\\''");
  return text;
}
