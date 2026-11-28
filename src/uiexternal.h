// uiexternal.h
//
// Copyright (c) 2019-2026 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#pragma once

#include <string>
#include <vector>

#include "uiviewbase.h"

class UiExternal : public UiViewBase
{
public:
  explicit UiExternal(Ui* p_Ui);

  void ExtEditor(const std::string& p_EditorCmd, std::wstring& p_ComposeMessageStr, int& p_ComposeMessagePos);
  void ExtPager(const std::string& p_Text);
  int ExtPartsViewer(const std::string& p_Path);
  int ExtHtmlViewer(const std::string& p_Path);
  int ExtHtmlPreview(const std::string& p_Path);
  int ExtMsgViewer(const std::string& p_Path);
  int ExtLinkOpen(const std::string& p_Url);
  std::vector<std::string> ExtFilePicker(const std::string& p_FilePickerCmd);
};
