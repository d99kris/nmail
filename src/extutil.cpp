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
#include "strutil.h"
#include "sysutil.h"

std::string ExtUtil::m_HtmlToTextConvertCmd;
bool ExtUtil::m_HtmlToTextLinks = false;
std::string ExtUtil::m_TextToHtmlConvertCmd;
std::string ExtUtil::m_PartsViewerCmd;
std::string ExtUtil::m_HtmlPreviewCmd;
std::string ExtUtil::m_HtmlViewerCmd;
std::string ExtUtil::m_MsgViewerCmd;
std::string ExtUtil::m_LinkOpenCmd;
std::string ExtUtil::m_PagerCmd;
std::string ExtUtil::m_EditorCmd;
std::string ExtUtil::m_SpellCmd;
std::string ExtUtil::m_FilePickerCmd;

std::string ExtUtil::GetHtmlToTextConvertCmd()
{
  return m_HtmlToTextConvertCmd;
}

void ExtUtil::SetHtmlToTextConvertCmd(const std::string& p_HtmlToTextConvertCmd)
{
  m_HtmlToTextConvertCmd = p_HtmlToTextConvertCmd;
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
  return m_TextToHtmlConvertCmd;
}

void ExtUtil::SetTextToHtmlConvertCmd(const std::string& p_TextToHtmlConvertCmd)
{
  m_TextToHtmlConvertCmd = p_TextToHtmlConvertCmd;
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
  if (!m_PartsViewerCmd.empty()) return m_PartsViewerCmd;

  static std::string defaultPartsViewerCmd = GetDefaultPartsViewerCmd();

  return defaultPartsViewerCmd;
}

void ExtUtil::SetPartsViewerCmd(const std::string& p_PartsViewerCmd)
{
  m_PartsViewerCmd = p_PartsViewerCmd;
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
  return m_PartsViewerCmd.empty();
}

std::string ExtUtil::GetHtmlViewerCmd()
{
  if (!m_HtmlViewerCmd.empty()) return m_HtmlViewerCmd;

  static std::string defaultHtmlViewerCmd = GetDefaultHtmlViewerCmd();

  return defaultHtmlViewerCmd;
}

void ExtUtil::SetHtmlViewerCmd(const std::string& p_HtmlViewerCmd)
{
  m_HtmlViewerCmd = p_HtmlViewerCmd;
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
  if (!m_HtmlPreviewCmd.empty()) return m_HtmlPreviewCmd;

  static std::string defaultHtmlPreviewCmd = GetDefaultHtmlPreviewCmd();

  return defaultHtmlPreviewCmd;
}

void ExtUtil::SetHtmlPreviewCmd(const std::string& p_HtmlPreviewCmd)
{
  m_HtmlPreviewCmd = p_HtmlPreviewCmd;
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
  return m_HtmlPreviewCmd.empty();
}

std::string ExtUtil::GetMsgViewerCmd()
{
  if (!m_MsgViewerCmd.empty()) return m_MsgViewerCmd;

  static std::string defaultMsgViewerCmd = GetDefaultMsgViewerCmd();

  return defaultMsgViewerCmd;
}

void ExtUtil::SetMsgViewerCmd(const std::string& p_MsgViewerCmd)
{
  m_MsgViewerCmd = p_MsgViewerCmd;
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
  return m_MsgViewerCmd.empty();
}

std::string ExtUtil::GetLinkOpenCmd()
{
  if (!m_LinkOpenCmd.empty()) return m_LinkOpenCmd;

  static std::string defaultLinkOpenCmd = GetDefaultLinkOpenCmd();

  return defaultLinkOpenCmd;
}

void ExtUtil::SetLinkOpenCmd(const std::string& p_LinkOpenCmd)
{
  m_LinkOpenCmd = p_LinkOpenCmd;
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
  return m_LinkOpenCmd.empty();
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

void ExtUtil::SetEditorCmd(const std::string& p_EditorCmd)
{
  m_EditorCmd = p_EditorCmd;
}

std::string ExtUtil::GetEditorCmd()
{
  if (!m_EditorCmd.empty()) return m_EditorCmd;

  return std::string(getenv("EDITOR") ? getenv("EDITOR") : "nano");
}

void ExtUtil::SetSpellCmd(const std::string& p_SpellCmd)
{
  m_SpellCmd = p_SpellCmd;
}

std::string ExtUtil::GetSpellCmd()
{
  static const std::string cmd = []()
  {
    std::string spellCheckCommand = m_SpellCmd;
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

void ExtUtil::SetPagerCmd(const std::string& p_PagerCmd)
{
  m_PagerCmd = p_PagerCmd;
}

std::string ExtUtil::GetPagerCmd()
{
  if (!m_PagerCmd.empty()) return m_PagerCmd;

  return std::string(getenv("PAGER") ? getenv("PAGER") : "less");
}

void ExtUtil::SetBrowserCmd(const std::string& p_BrowserCmd)
{
  if (p_BrowserCmd.empty()) return;

  setenv("BROWSER", p_BrowserCmd.c_str(), 1);
}

void ExtUtil::SetFilePickerCmd(const std::string& p_FilePickerCmd)
{
  m_FilePickerCmd = p_FilePickerCmd;
}

std::string ExtUtil::GetFilePickerCmd()
{
  return m_FilePickerCmd;
}

std::string ExtUtil::EscapeSingleQuote(const std::string& p_Str)
{
  // for use inside single-quoted shell argument
  std::string text = p_Str;
  StrUtil::ReplaceString(text, "'", "'\\''");
  return text;
}
