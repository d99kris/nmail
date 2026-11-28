// extutil.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>

// External commands (viewers, editors, converters) and their settings.
class ExtUtil
{
public:
  static std::string GetHtmlToTextConvertCmd();
  static void SetHtmlToTextConvertCmd(const std::string& p_HtmlToTextConvertCmd);
  static size_t GetHtmlToTextConvertCmdHash();
  static void SetHtmlToTextLinks(bool p_HtmlToTextLinks);
  static std::string ConvertHtmlToText(const std::string& p_Html);
  static std::string GetTextToHtmlConvertCmd();
  static void SetTextToHtmlConvertCmd(const std::string& p_TextToHtmlConvertCmd);
  static std::string ConvertTextToHtml(const std::string& p_Text);
  static std::string GetPartsViewerCmd();
  static void SetPartsViewerCmd(const std::string& p_PartsViewerCmd);
  static std::string GetDefaultPartsViewerCmd();
  static bool IsDefaultPartsViewerCmd();
  static std::string GetHtmlViewerCmd();
  static void SetHtmlViewerCmd(const std::string& p_HtmlViewerCmd);
  static std::string GetDefaultHtmlViewerCmd();
  static std::string GetSystemOpenCmd();
  static bool IsHtmlViewerSystemOpenCmd();
  static std::string GetHtmlPreviewCmd();
  static void SetHtmlPreviewCmd(const std::string& p_HtmlPreviewCmd);
  static std::string GetDefaultHtmlPreviewCmd();
  static bool IsDefaultHtmlPreviewCmd();
  static std::string GetMsgViewerCmd();
  static void SetMsgViewerCmd(const std::string& p_MsgViewerCmd);
  static std::string GetDefaultMsgViewerCmd();
  static bool IsDefaultMsgViewerCmd();
  static std::string GetLinkOpenCmd();
  static void SetLinkOpenCmd(const std::string& p_LinkOpenCmd);
  static std::string GetDefaultLinkOpenCmd();
  static bool IsDefaultLinkOpenCmd();
  static std::string MakeCmd(const std::string& p_Cmd, const std::string& p_Arg);
  static void SetEditorCmd(const std::string& p_EditorCmd);
  static std::string GetEditorCmd();
  static void SetSpellCmd(const std::string& p_SpellCmd);
  static std::string GetSpellCmd();
  static void SetPagerCmd(const std::string& p_PagerCmd);
  static std::string GetPagerCmd();
  static void SetBrowserCmd(const std::string& p_BrowserCmd);
  static void SetFilePickerCmd(const std::string& p_FilePickerCmd);
  static std::string GetFilePickerCmd();
  static std::string EscapeSingleQuote(const std::string& p_Str);

private:
  static std::string m_HtmlToTextConvertCmd;
  static bool m_HtmlToTextLinks;
  static std::string m_TextToHtmlConvertCmd;
  static std::string m_PartsViewerCmd;
  static std::string m_HtmlPreviewCmd;
  static std::string m_HtmlViewerCmd;
  static std::string m_MsgViewerCmd;
  static std::string m_LinkOpenCmd;
  static std::string m_PagerCmd;
  static std::string m_EditorCmd;
  static std::string m_SpellCmd;
  static std::string m_FilePickerCmd;
};
