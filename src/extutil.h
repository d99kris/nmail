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
  static size_t GetHtmlToTextConvertCmdHash();
  static void SetHtmlToTextLinks(bool p_HtmlToTextLinks);
  static std::string ConvertHtmlToText(const std::string& p_Html);
  static std::string GetTextToHtmlConvertCmd();
  static std::string ConvertTextToHtml(const std::string& p_Text);
  static std::string GetPartsViewerCmd();
  static std::string GetDefaultPartsViewerCmd();
  static bool IsDefaultPartsViewerCmd();
  static std::string GetHtmlViewerCmd();
  static std::string GetDefaultHtmlViewerCmd();
  static std::string GetSystemOpenCmd();
  static bool IsHtmlViewerSystemOpenCmd();
  static std::string GetHtmlPreviewCmd();
  static std::string GetDefaultHtmlPreviewCmd();
  static bool IsDefaultHtmlPreviewCmd();
  static std::string GetMsgViewerCmd();
  static std::string GetDefaultMsgViewerCmd();
  static bool IsDefaultMsgViewerCmd();
  static std::string GetLinkOpenCmd();
  static std::string GetDefaultLinkOpenCmd();
  static bool IsDefaultLinkOpenCmd();
  static std::string MakeCmd(const std::string& p_Cmd, const std::string& p_Arg);
  static std::string GetEditorCmd();
  static std::string GetSpellCmd();
  static std::string GetPagerCmd();
  static void SetBrowserCmd(const std::string& p_BrowserCmd);
  static std::string GetFilePickerCmd();
  static std::string EscapeSingleQuote(const std::string& p_Str);

private:
  static bool m_HtmlToTextLinks;
};
